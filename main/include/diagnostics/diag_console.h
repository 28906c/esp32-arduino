#ifndef DIAG_CONSOLE_H
#define DIAG_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化服务终端，M0 仅注册内置命令 */
void diag_console_init(void);

/* 执行一条命令，返回 0 表示成功 */
int diag_console_execute(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_CONSOLE_H */
