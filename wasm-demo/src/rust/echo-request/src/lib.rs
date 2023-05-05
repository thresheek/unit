//extern crate rust_unit_wasm;
use rust_unit_wasm::*;

// Buffer of some size to store the copy of the request

const REQUEST_BUF: *mut *mut u8 = std::ptr::null_mut();
//static request_buf: Vec<u8> = Vec::<u8>::new();

#[no_mangle]
pub extern "C" fn luw_request_handler(addr: *mut u8) -> i32 {
    // Need a initalization
    // What is luw_ctx_t?
    let mut ctx_: luw_ctx_t = luw_ctx_t{
        addr: std::ptr::null_mut(),
        mem: std::ptr::null_mut(),
        req: std::ptr::null_mut(),
        resp: std::ptr::null_mut(),
        resp_hdr: std::ptr::null_mut(),
        resp_offset: 0,
        req_buf: std::ptr::null_mut(),
        hdrp: std::ptr::null_mut(),
        reqp: std::ptr::null_mut(),
    };

    let ctx: *mut luw_ctx_t = &mut ctx_;

    unsafe {
      // init context with the context structure, the starting addr of the linear memory space and
      // an offset of 4096 bytes. The response data will be stored AFTER the offset of 4096 bytes.
      // This will leave some space for request headers.

      luw_init_ctx(ctx, addr, 4096);

      // Allocate Memory and copy the request data.
      luw_set_req_buf(ctx, REQUEST_BUF, luw_srb_flags_t_LUW_SRB_ALLOC | luw_srb_flags_t_LUW_SRB_FULL_SIZE);

      // Define the Response Body Text.
      let response = "Hello World\n";
      luw_mem_writep(ctx, response.as_ptr() as *const i8);

      // Init Response Headers
      // needs the context, number of headers about to add as well as the offset.
      // As the response headers are at the beginnging of the memory offset of 4096 we put a 0
      // here.

      luw_http_init_headers(ctx, 2, 0);
      luw_http_add_header(ctx, 0, "Content-Type".as_ptr() as *const i8, "text/html".as_ptr() as *const i8);
      luw_http_add_header(ctx, 1, "Content-Length".as_ptr() as *const i8, response.len() as *const i8);

      //send means: Call the function from Unit(Host)
      luw_http_send_headers(ctx);

      luw_http_send_response(ctx);
      luw_http_response_end();

    }

    return 0;
}
