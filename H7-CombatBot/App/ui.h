#ifndef __UI_H
#define __UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void UI_ShowMain(void);         /* LCD 显示主界面 */
void UI_ShowTarget(void);       /* LCD 显示目标色块位置信息 */

#ifdef __cplusplus
}
#endif

#endif /* __UI_H */
