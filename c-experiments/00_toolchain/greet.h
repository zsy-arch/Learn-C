/* greet.h —— 头文件只放「声明」，不放「定义」 */
#ifndef GREET_H          /* include guard：防止同一个头文件被重复展开 */
#define GREET_H

/* 声明(declaration)：告诉编译器「有这么个函数，长这样」 */
void greet(const char *who);

/* 声明一个在别处定义的全局变量 */
extern int g_greet_count;

#endif /* GREET_H */
