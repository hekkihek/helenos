/*
 * Copyright (c) 2011 Jiri Svoboda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup tcp
 * @{
 */

/**
 * @file TCP entry points (close to those defined in the RFC)
 */

#include <fibril_synch.h>
#include <io/log.h>
#include <macros.h>
#include <mem.h>
#include "conn.h"
#include "state.h"
#include "tcp_type.h"
#include "tqueue.h"

/*
 * User calls
 */

/** OPEN user call
 *
 * @param lport		Local port
 * @param fsock		Foreign socket
 * @param acpass	Active/passive
 * @param conn		Connection
 */
void tcp_uc_open(uint16_t lport, tcp_sock_t *fsock, acpass_t acpass,
    tcp_conn_t **conn)
{
	tcp_conn_t *nconn;
	tcp_sock_t lsock;

	log_msg(LVL_DEBUG, "tcp_uc_open(%" PRIu16 ", %p, %s, %p)",
	    lport, fsock, acpass == ap_active ? "active" : "passive",
	    conn);

	lsock.port = lport;
	lsock.addr.ipv4 = 0x7f000001;

	nconn = tcp_conn_new(&lsock, fsock);
	tcp_conn_add(nconn);

	if (acpass == ap_active) {
		/* Synchronize (initiate) connection */
		tcp_conn_sync(nconn);
	}

	*conn = nconn;
}

/** SEND user call */
void tcp_uc_send(tcp_conn_t *conn, void *data, size_t size, xflags_t flags)
{
	size_t buf_free;
	size_t xfer_size;

	log_msg(LVL_DEBUG, "tcp_uc_send()");

	while (size > 0) {
		buf_free = conn->snd_buf_size - conn->snd_buf_used;
		while (buf_free == 0)
			tcp_tqueue_new_data(conn);

		xfer_size = min(size, buf_free);

		/* Copy data to buffer */
		memcpy(conn->snd_buf + conn->snd_buf_used, data, xfer_size);
		data += xfer_size;
		conn->snd_buf_used += xfer_size;
		size -= xfer_size;
	}

	tcp_tqueue_new_data(conn);
}

/** RECEIVE user call */
void tcp_uc_receive(tcp_conn_t *conn, void *buf, size_t size, size_t *rcvd,
    xflags_t *xflags)
{
	size_t xfer_size;

	log_msg(LVL_DEBUG, "tcp_uc_receive()");

	fibril_mutex_lock(&conn->rcv_buf_lock);

	/* Wait for data to become available */
	while (conn->rcv_buf_used == 0) {
		log_msg(LVL_DEBUG, "tcp_uc_receive() - wait for data");
		fibril_condvar_wait(&conn->rcv_buf_cv, &conn->rcv_buf_lock);
	}

	/* Copy data from receive buffer to user buffer */
	xfer_size = min(size, conn->rcv_buf_used);
	memcpy(buf, conn->rcv_buf, xfer_size);
	*rcvd = xfer_size;

	/* Remove data from receive buffer */
	memmove(conn->rcv_buf, conn->rcv_buf + xfer_size, conn->rcv_buf_used -
	    xfer_size);
	conn->rcv_buf_used -= xfer_size;
	conn->rcv_wnd += xfer_size;

	fibril_mutex_unlock(&conn->rcv_buf_lock);

	/* TODO */
	*xflags = 0;

	/* Send new size of receive window */
	tcp_tqueue_ctrl_seg(conn, CTL_ACK);

	log_msg(LVL_DEBUG, "tcp_uc_receive() - returning %zu bytes",
	    xfer_size);
}

/** CLOSE user call */
void tcp_uc_close(tcp_conn_t *conn)
{
	log_msg(LVL_DEBUG, "tcp_uc_close()");
}

/** ABORT user call */
void tcp_uc_abort(tcp_conn_t *conn)
{
	log_msg(LVL_DEBUG, "tcp_uc_abort()");
}

/** STATUS user call */
void tcp_uc_status(tcp_conn_t *conn, tcp_conn_status_t *cstatus)
{
	log_msg(LVL_DEBUG, "tcp_uc_status()");
}


/*
 * Arriving segments
 */

/** Segment arrived */
void tcp_as_segment_arrived(tcp_sockpair_t *sp, tcp_segment_t *seg)
{
	tcp_conn_t *conn;

	log_msg(LVL_DEBUG, "tcp_as_segment_arrived()");

	conn = tcp_conn_find(sp);
	if (conn != NULL) {
		tcp_conn_segment_arrived(conn, seg);
	} else {
		tcp_unexpected_segment(sp, seg);
	}
}

/*
 * Timeouts
 */

/** User timeout */
void tcp_to_user(void)
{
	log_msg(LVL_DEBUG, "tcp_to_user()");
}

/** Retransmission timeout */
void tcp_to_retransmit(void)
{
	log_msg(LVL_DEBUG, "tcp_to_retransmit()");
}

/** Time-wait timeout */
void tcp_to_time_wait(void)
{
	log_msg(LVL_DEBUG, "tcp_to_time_wait()");
}

/**
 * @}
 */
