// gpiod_test_v1.c
// 编译: gcc -o gpiodtest gpiodtest.c -lgpiod

#include <gpiod.h> // Requires libgpiod-dev (v1.x)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for sleep

#ifndef GPIOD_TEST_CONSUMER
#define GPIOD_TEST_CONSUMER "gpiod_test_v1"
#endif

int main(void) {
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    int ret;

    // 1. 打开 GPIO 芯片
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        perror("无法打开 GPIO 芯片 'gpiochip0'");
        fprintf(stderr, "请检查芯片名称是否正确，以及权限设置。\n");
        fprintf(stderr, "尝试运行 'gpiodetect' 查看可用芯片。\n");
        return EXIT_FAILURE;
    }

    printf("成功打开芯片: %s (%s)\n", gpiod_chip_name(chip), gpiod_chip_label(chip));
    printf("线路总数: %u\n", gpiod_chip_num_lines(chip));

    // 2. 获取特定线路 (例如线路 17，你需要根据实际情况修改)
    // *** 重要：请将下面的 17 替换为你实际要测试的 GPIO 引脚号 ***
    unsigned int test_line_offset = 22;
    line = gpiod_chip_get_line(chip, test_line_offset);
    if (!line) {
        perror("无法获取线路");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // 3. 请求线路作为输出
    ret = gpiod_line_request_output(line, GPIOD_TEST_CONSUMER, 0); // 初始值为 0 (低电平)
    if (ret < 0) {
        perror("无法请求线路作为输出");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    printf("成功请求线路 %u 作为输出。\n", test_line_offset);

    // 4. 切换线路状态 (闪烁)
    printf("正在切换线路 %u 的状态 (闪烁)...\n", test_line_offset);
    for (int i = 0; i < 5; i++) {
        // 设置为高电平
        ret = gpiod_line_set_value(line, 1);
        if (ret < 0) {
            perror("设置线路值为 1 失败");
            gpiod_line_release(line);
            gpiod_chip_close(chip);
            return EXIT_FAILURE;
        }
        printf("线路 %u 设置为 高电平 (1)\n", test_line_offset);
        sleep(1); // 延迟 1 秒

        // 设置为低电平
        ret = gpiod_line_set_value(line, 0);
        if (ret < 0) {
            perror("设置线路值为 0 失败");
            gpiod_line_release(line);
            gpiod_chip_close(chip);
            return EXIT_FAILURE;
        }
        printf("线路 %u 设置为 低电平 (0)\n", test_line_offset);
        sleep(1); // 延迟 1 秒
    }

    // 5. 释放线路和关闭芯片
    gpiod_line_release(line);
    printf("线路 %u 已释放。\n", test_line_offset);

    gpiod_chip_close(chip);
    printf("芯片已关闭。\n");

    printf("GPIO 测试完成。\n");
    return EXIT_SUCCESS;
}

