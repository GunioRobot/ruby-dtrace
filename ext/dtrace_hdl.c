/* Ruby-Dtrace
 * (c) 2007 Chris Andrews <chris@nodnol.org>
 */

#include "dtrace_api.h"

RUBY_EXTERN VALUE eDtraceException;
RUBY_EXTERN VALUE cDtraceProbe;
RUBY_EXTERN VALUE cDtraceProgram;
RUBY_EXTERN VALUE cDtraceRecDesc;
RUBY_EXTERN VALUE cDtraceProbeData;
RUBY_EXTERN VALUE cDtraceBufData;

void dtrace_hdl_free (void *handle)
{
  dtrace_close(handle);
}

VALUE dtrace_hdl_alloc(VALUE klass)
{
  dtrace_hdl_t *handle;
  int err;
  VALUE obj;
  
  handle = dtrace_open(DTRACE_VERSION, 0, &err);
  
  if (handle) {
    /*
     * Leopard's DTrace requires symbol resolution to be 
     * switched on explicitly 
     */ 
#ifdef __APPLE__
    (void) dtrace_setopt(handle, "stacksymbols", "enabled");
#endif

    obj = Data_Wrap_Struct(klass, 0, dtrace_hdl_free, handle);
    return obj;
  }
  else {
    rb_raise(eDtraceException, "unable to open dtrace (not root?)");
  }
}

/* :nodoc: */
VALUE dtrace_init(VALUE self)
{
  dtrace_hdl_t *handle;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  if (handle)
    return self;
  else
    return Qnil;
}

int _dtrace_next_probe(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp, void *arg)
{
  VALUE probe;

  probe = Data_Wrap_Struct(cDtraceProbe, 0, NULL, (dtrace_probedesc_t *)pdp);

  rb_yield(probe);
  return 0;
}

/*
 * Yields each probe found on the system. 
 * (equivalent to dtrace -l)
 *
 * Each probe is represented by a DtraceProbe object
 */
VALUE dtrace_each_probe(VALUE self)
{
  dtrace_hdl_t *handle;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  (void) dtrace_probe_iter(handle, NULL, _dtrace_next_probe, NULL);

  return self;
}

/*
 * Compile a D program. 
 *
 * Arguments:
 * * The program text to compile
 * * (Optionally) any arguments required by the program
 *
 * Raises a DtraceException if the program cannot be compiled.
 */
VALUE dtrace_strcompile(int argc, VALUE *argv, VALUE self)
{
  dtrace_hdl_t *handle;
  dtrace_prog_t *program;
  VALUE dtrace_program;

  VALUE dtrace_text;
  int dtrace_argc;
  VALUE dtrace_argv_array;

  char **dtrace_argv;
  int i;

  rb_scan_args(argc, argv, "1*", &dtrace_text, &dtrace_argv_array);

  dtrace_argc = FIX2INT(rb_funcall(dtrace_argv_array, rb_intern("length"), 0));
  dtrace_argv = ALLOC_N(char *, dtrace_argc + 1);
  for (i = 0; i < dtrace_argc; i++) {
    dtrace_argv[i + 1] = STR2CSTR(rb_ary_entry(dtrace_argv_array, i));
  }

  dtrace_argv[0] = "ruby";
  dtrace_argc++;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  program = dtrace_program_strcompile(handle, STR2CSTR(dtrace_text),
				      DTRACE_PROBESPEC_NAME, DTRACE_C_PSPEC, 
				      dtrace_argc, dtrace_argv);

  if (!program) {
    rb_raise(eDtraceException, dtrace_errmsg(handle, dtrace_errno(handle)));
    return Qnil;
  }
  else {
    dtrace_program = Data_Wrap_Struct(cDtraceProgram, 0, NULL, program);
    rb_iv_set(dtrace_program, "@dtrace", self);
    return dtrace_program;
  }
}

/*
 * Start tracing. Must be called once a program has been successfully
 * compiled and executed.
 * 
 * Raises a DtraceException on any error.
 */
VALUE dtrace_hdl_go(VALUE self)
{
  dtrace_hdl_t *handle;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  if (dtrace_go(handle) < 0) 
    rb_raise(eDtraceException, dtrace_errmsg(handle, dtrace_errno(handle)));
    
  return Qnil;
}

/* 
 * Returns the status of the DTrace handle.
 *
 * Status values are defined as:
 * 
 * * 0 - none
 * * 1 - ok
 * * 4 - stopped
 */
VALUE dtrace_hdl_status(VALUE self)
{
  dtrace_hdl_t *handle;
  int status;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  if ((status = dtrace_status(handle)) < 0) 
    rb_raise(eDtraceException, dtrace_errmsg(handle, dtrace_errno(handle)));
    
  return INT2FIX(status);
}

/* 
 * Set an option on the DTrace handle. 
 * 
 * Options which may be set:
 * 
 * * aggsize
 * * bufsize
 */
VALUE dtrace_hdl_setopt(VALUE self, VALUE key, VALUE value)
{
  dtrace_hdl_t *handle;
  int ret;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  
  if (NIL_P(value)) {
    ret = dtrace_setopt(handle, STR2CSTR(key), 0);
  }
  else {
    ret = dtrace_setopt(handle, STR2CSTR(key), STR2CSTR(value));
  }

  if (ret < 0) 
    rb_raise(eDtraceException, dtrace_errmsg(handle, dtrace_errno(handle)));
    
  return Qnil;
}

/* Stop tracing. 
 *
 * Must be called after go has been called to start tracing.
 */
VALUE dtrace_hdl_stop(VALUE self)
{
  dtrace_hdl_t *handle;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  if (dtrace_stop(handle) < 0) 
    rb_raise(eDtraceException, dtrace_errmsg(handle, dtrace_errno(handle)));
    
  return Qnil;
}

/* 
 * Return the most recent DTrace error.
 */
VALUE dtrace_hdl_error(VALUE self)
{
  dtrace_hdl_t *handle;
  const char *error_string;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  error_string = dtrace_errmsg(handle, dtrace_errno(handle));
  return rb_str_new2(error_string);
}

/*
 * Sleep until we need to wake up to honour D options controlling
 * consumption rates.
 */
VALUE dtrace_hdl_sleep(VALUE self)
{
  dtrace_hdl_t *handle;

  Data_Get_Struct(self, dtrace_hdl_t, handle);
  dtrace_sleep(handle);
  return Qnil;
}

static int _probe_consumer(const dtrace_probedata_t *data, void *arg)
{
  VALUE proc;
  dtrace_work_handlers_t handlers;
  VALUE probedata;

  handlers = *(dtrace_work_handlers_t *) arg;
  proc = handlers.probe;

  if (!NIL_P(proc)) {
    probedata = Data_Wrap_Struct(cDtraceProbeData, 0, NULL, (dtrace_probedata_t *)data);
    rb_iv_set(probedata, "@handle", handlers.handle);
    rb_funcall(proc, rb_intern("call"), 1, probedata);
  }

  return (DTRACE_CONSUME_THIS);
}

static int _rec_consumer(const dtrace_probedata_t *data, const dtrace_recdesc_t *rec, void *arg)
{
  VALUE proc;
  dtrace_work_handlers_t handlers;
  VALUE recdesc;

  dtrace_actkind_t act;
  uintptr_t addr;

  if (rec == NULL)
    return (DTRACE_CONSUME_NEXT);

  handlers = *(dtrace_work_handlers_t *) arg;
  proc = handlers.rec;

  if (!NIL_P(proc)) {
    recdesc = Data_Wrap_Struct(cDtraceRecDesc, 0, NULL, (dtrace_recdesc_t *)rec);
    rb_iv_set(recdesc, "@handle", handlers.handle);
    rb_funcall(proc, rb_intern("call"), 1, recdesc);
  }
  
  act = rec->dtrd_action;
  addr = (uintptr_t)data->dtpda_data;
  
  if (act == DTRACEACT_EXIT)
    return (DTRACE_CONSUME_NEXT);
  
  return (DTRACE_CONSUME_THIS);
}

static int _buf_consumer(const dtrace_bufdata_t *bufdata, void *arg)
{
  VALUE proc;
  VALUE dtracebufdata;

  proc = (VALUE)arg;

  if (!NIL_P(proc)) {
    dtracebufdata = Data_Wrap_Struct(cDtraceBufData, 0, NULL, (dtrace_bufdata_t *)bufdata);
    rb_funcall(proc, rb_intern("call"), 1, dtracebufdata);
  }

  return (DTRACE_HANDLE_OK);
}

/*
 * Process any data waiting from the D program.
 * 
 * Takes a Proc to which DtraceProbeData objects will be yielded, and
 * an optional second Proc to which DtraceRecDesc objects will be
 * yielded.
 *
 */
VALUE dtrace_hdl_work(int argc, VALUE *argv, VALUE self)
{
  dtrace_hdl_t *handle;
  dtrace_workstatus_t status;
  dtrace_work_handlers_t handlers;
  VALUE probe_consumer_proc;
  VALUE rec_consumer_proc;
  
  Data_Get_Struct(self, dtrace_hdl_t, handle);

  /* handle args - probe_consumer_proc is mandatory, rec_consumer_proc
     is optional */
  rb_scan_args(argc, argv, "11", &probe_consumer_proc, &rec_consumer_proc);

  /* fill out the handlers struct */
  handlers.probe  = probe_consumer_proc;
  handlers.rec    = rec_consumer_proc;
  handlers.handle = self;

  status = dtrace_work(handle, NULL, _probe_consumer, _rec_consumer, &handlers);

  return INT2FIX(status);
}  

/*
 * Set up the buffered output handler for this handle.
 */
VALUE dtrace_hdl_buf_consumer(VALUE self, VALUE buf_consumer)
{
  dtrace_hdl_t *handle;
  Data_Get_Struct(self, dtrace_hdl_t, handle);

  /* attach the buffered output handler */
  if (dtrace_handle_buffered(handle, &_buf_consumer, (void *)buf_consumer) == -1) {
    rb_raise(eDtraceException, "failed to establish buffered handler");
  }

  return Qnil;
}
