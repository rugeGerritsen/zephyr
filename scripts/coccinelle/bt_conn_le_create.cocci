/// Correctly unreference connection context obtained with bt_conn_le_create
///
//# This makes an effort to find cases where an application forgets to
//# unreference a connection object.
//# This leads to subtle bugs.
//
// Confidence: Low
// Copyright: (C) 2024 Nordic Semiconductor ASA
// Comments:
// Options: --no-includes --include-headers

virtual patch
virtual context
virtual org
virtual report

@r1@
local idexpression conn;
identifier e;
position p;
statement S1, S2;
@@
* e = bt_conn_le_create(..., &conn@p);
... when != bt_conn_unref(conn)
    when != if (e == 0) { <+... bt_conn_unref(conn) ...+> } else S1
    when != if (e) S2 else { <+... bt_conn_unref(conn) ...+> }
return;

@r2@
local idexpression conn;
identifier e;
type T;
position p;
statement S1, S2;
@@
* T e = bt_conn_le_create(..., &conn@p);
... when != bt_conn_unref(conn)
    when != if (e == 0) { <+... bt_conn_unref(conn) ...+> } else S1
    when != if (e) S2 else { <+... bt_conn_unref(conn) ...+> }
return;

@script:python depends on org@
p << r1.p;
p << r2.p;
@@

cocci.print_main("Missing bt_conn_unref for connection ref obtained at", p)

@script:python depends on report@
p << r1.p;
p << r2.p;
@@

msg = "ERROR: Missing bt_conn_unref for connection ref obtained at"
coccilib.report.print_report(p[0],msg)
