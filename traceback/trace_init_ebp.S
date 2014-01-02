/** @file trace_init_ebp.S
 *  @brief The function to read the initial ebp
 *
 *  @author Zhuolin Liu (zhuolinl)
 *  @bug No known bugs
 */
.global trace_init_ebp

trace_init_ebp:
    pushl   %ebp
    movl    %esp,%ebp
    movl    %ebp,%eax
    movl    %ebp,%esp
    popl    %ebp
    ret