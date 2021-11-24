#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "include/extcmd.h"
#include "include/args.h"
#include "include/selector.h"
#include "include/main.h"
#include "include/logger.h"
#include "include/proxytcp.h"

/* ==================================================== */
/*                     PROTOTYPES                       */
/* ==================================================== */

int
write_buffer_to_filter(struct selector_key *key, buffer* buff);

int
read_ext_cmd(struct selector_key *key, buffer* buff);

void
env_var_init(void);

/* ==================================================== */
/*                   IMPLEMENTATION                     */
/* ==================================================== */

//
//int
//write_buffer_to_filter(struct selector_key *key, buffer* buff) {
//    struct extern_cmd * filter = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;
//    buffer *sb = filter->wb;
//    uint8_t *b;
//    size_t count;
//    ssize_t n = 0;
//    size_t i = 0;
//
//    char c;
//    while (buffer_can_read(buff) && n == 0) {
//        c = buffer_read(buff);
//        i++;
//        if (c == '\n') {
//            n = i;
//        }
//        buffer_write(sb, c);
//    }
//
//    if (n == 0) {
//        return 0;
//    }
//
//    b = buffer_read_ptr(sb, &count);
//    n = write(ATTACHMENT(key)->w_to_filter_fds[WRITE], b, count);
//    buffer_reset(sb);
//
//    return n;
//}

//int
//read_ext_cmd(struct selector_key *key, buffer* buff) {
//    int bytes_read = read(ATTACHMENT(key)->`r_from_filter_fds`[READ], buff, strlen( (char*)buff) );
//    if( bytes_read < 0 )
//    log(ERROR, "read_ext_cmd: reading from file descriptor failed");
//    return bytes_read;
//}
