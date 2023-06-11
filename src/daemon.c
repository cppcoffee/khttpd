#include <linux/net.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/signal.h>
#include <linux/slab.h>

#include "http_request.h"

// defined in main.c
extern char *root_dir;
extern size_t root_dir_len;

static struct workqueue_struct *http_workqueue;
static struct task_struct *accept_thread;
static struct kmem_cache *worker_cachep;

static int accept_handler(void *arg)
{
	int err;
	struct http_worker *worker;
	struct socket *new_conn;
	struct socket *listen_socket = (struct socket *)arg;

	allow_signal(SIGKILL);
	allow_signal(SIGTERM);

	while (!kthread_should_stop()) {
		err = kernel_accept(listen_socket, &new_conn, 0);
		if (err < 0) {
			if (signal_pending(current)) {
				break;
			}

			pr_err("kernel_accept() error: %d\n", err);
			continue;
		}

		worker = kmem_cache_zalloc(worker_cachep, GFP_KERNEL);
		if (worker == NULL) {
			pr_err("kmem_cache_alloc(http_Worker) error\n");
			continue;
		}

		// TODO: use mempool
		worker->max_path_length = PATH_MAX;

		worker->absolute_path.data =
			kmalloc(worker->max_path_length, GFP_KERNEL);
		if (worker->absolute_path.data == NULL) {
			pr_err("kmalloc(absolute_path) error\n");

			kmem_cache_free(worker_cachep, worker);
			continue;
		}

		worker->socket = new_conn;
		worker->worker_cachep = worker_cachep;
		worker->root_dir.data = root_dir;
		worker->root_dir.len = root_dir_len;

		INIT_WORK(&worker->work, http_request_handler);
		queue_work(http_workqueue, &worker->work);
	}

	return 0;
}

void free_worker(struct http_worker *worker)
{
	// TODO: use mempool
	kfree(worker->absolute_path.data);

	sock_release(worker->socket);
	kmem_cache_free(worker->worker_cachep, worker);
}

int http_daemon_start(struct socket *listen_socket)
{
	int rc = 0;

	worker_cachep = kmem_cache_create("http_worker_pool",
					  sizeof(struct http_worker), 0,
					  SLAB_HWCACHE_ALIGN, NULL);
	if (worker_cachep == NULL) {
		pr_err("kmem_cache_create http worker pool failed\n");

		rc = -ENOMEM;
		goto fail;
	}

	http_workqueue = alloc_workqueue("http_workerqueue", WQ_UNBOUND, 0);
	if (http_workqueue == NULL) {
		pr_err("alloc_workqueue failed\n");

		rc = -ENOMEM;
		goto fail;
	}

	accept_thread =
		kthread_run(accept_handler, listen_socket, KBUILD_MODNAME);
	if (IS_ERR(accept_thread)) {
		pr_err("kthread_run failed\n");

		rc = PTR_ERR(accept_thread);
		goto fail;
	}

	return 0;

fail:

	if (worker_cachep != NULL) {
		kmem_cache_destroy(worker_cachep);
	}

	if (http_workqueue != NULL) {
		destroy_workqueue(http_workqueue);
	}

	return rc;
}

void http_daemon_stop(void)
{
	send_sig(SIGTERM, accept_thread, 0);
	kthread_stop(accept_thread);

	flush_workqueue(http_workqueue);
	destroy_workqueue(http_workqueue);

	kmem_cache_destroy(worker_cachep);
}
