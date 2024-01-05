/**
 * @file test1.c
 * @brief Модуль ядра, который складывает два float числа.
 *        Модуль создаёт в sysfs три файла: float_1, float_2, result.
 *        Файлы float_1 и float_2 доступны для чтения и записи,
 *        в них записываются float числа, по умолчанию в них 0.0.
 *        Файл result доступен только для чтения, при его вызове
 *        в терминал выводится результат сложения float_1 и float_2.
 *        Необходимо учитывать, что модуль может быть запущен без FPU.
 * @note  Числа представлены в формате 22.10 (int32).
 *        Допустимый диапазон: (-1000000; +1000000).
 *        Точность: 0.001.
 *
 * @date 2023-10-27
 * @author K.M.Maximov
 */
//============================================================================//
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/printk.h>

#define   FIXEDPT_WBITS  22
#include "fixedptc.h"

static fixedpt str2fxpt(const char *s);

/**
 * @brief Размер буфера для хранения числа в виде строки.
 * 
 */
#define MAX_VAR_SIZE       32
/**
 * @brief Макрос для создания атрибута.
 *        Переопределён для обхода ограничений доступа RW для всех.
 * 
 */
#define MY_ATTR(_name, _mode, _show, _store) {              \
	.attr = {.name = __stringify(_name), .mode = _mode},    \
	.show	= _show,                                        \
	.store	= _store,                                       \
}

/**
 * @brief Переменные (строки) для хранения содержимого
 *        одноимённых атрибутов/файлов в /sys/test_1/
 * 
 */
static char float_1[MAX_VAR_SIZE] = { '0', '\0' };
static char float_2[MAX_VAR_SIZE] = { '0', '\0' };
static char  result[MAX_VAR_SIZE] = { '0', '\0' };

//============================================================================//
/**
 * @brief Метод для чтения содержимого атрибута/файла в /sys/test_1/
 * @note  Имя атрибута определяется по его имени attr->attr.name
 * 
 * @param kobj - объект ядра
 * @param attr - наш атрибут
 * @param buffer - буфер для записи данных
 * @return ssize_t - кол-во записанных байт
 */
static ssize_t my_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *ptr = "";
	pr_info("Test_1: my_show() [%s] \n", attr->attr.name);

	if (strcmp(attr->attr.name, "float_1") == 0)
	{
		ptr = float_1;
	}
	else if (strcmp(attr->attr.name, "float_2") == 0)
	{
		ptr = float_2;
	}
	else if (strcmp(attr->attr.name, "result") == 0)
	{
		ptr = result;
	}
	else
	{
		return 0;
	}
	return sprintf(buf, "%s", ptr);
}

//============================================================================//
/**
 * @brief Метод для записи/хранения поступивших данных в атрибут/файл.
 * @note  Данные (строка) переводится в число (fixedfp) и вычисляется сумма.
 * 
 * @param kobj - объект ядра
 * @param attr - атрибут для записи
 * @param buffer - буфер для записи
 * @param count - размер буфера
 * @return ssize_t - кол-во записанных байт
 */
static ssize_t my_store(struct kobject *kobj, struct kobj_attribute *attr,
                        const char *buffer, size_t count)
{
	fixedpt f1, f2, res;
	pr_info("Test_1: my_store() [%s] \n", attr->attr.name);

	if (strcmp(attr->attr.name, "float_1") == 0)
	{
		sscanf(buffer, "%15s", float_1);
	}
	else if (strcmp(attr->attr.name, "float_2") == 0)
	{
		sscanf(buffer, "%15s", float_2);
	}
	else
	{
		return 0;
	}

	f1 = str2fxpt(float_1);
	f2 = str2fxpt(float_2);
	res = f1 + f2;

	fixedpt_str(f1, float_1, 4);
	fixedpt_str(f2, float_2, 4);
	fixedpt_str(res, result, 4);

	return count;
}

//============================================================================//
/**
 * @brief Объявления атрибутов/файлов, создаваемых в фуйловой системе /sys
 * @note  Атрибуты объеденины в группу для компактного управления.
 * 
 */
static struct kobj_attribute my_attr_1 = MY_ATTR(float_1, 0666, my_show, my_store);
static struct kobj_attribute my_attr_2 = MY_ATTR(float_2, 0666, my_show, my_store);
static struct kobj_attribute my_attr_3 = MY_ATTR( result, 0444, my_show, NULL);
/*
// RO = 0444 RW = 0644 WO = 0200
// TODO: my system do not allow 0666 groups...
static struct attribute *attrs[] = {
	&my_attr_1.attr,
	&my_attr_2.attr,
	&my_attr_3.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};
*/

//============================================================================//
static struct kobject *my_module;
/**
 * @brief Метод для инициализации модуля ядра и создания атрибутов/файлов /sys
 * 
 * @return int - возвращает результат работы, 0 - успех.
 */
static int __init my_init(void)
{
	int error1, error2, error3;
	pr_info("Test_1: my_init()\n");

	my_module = kobject_create_and_add("test_1", NULL);
	if (!my_module)
	{
		pr_alert("Test_1: failed to create subfolder kobject in /sys/\n");
		return -ENOMEM;
	}

	//error = sysfs_create_group(my_module, &attr_group);
	error1 = sysfs_create_file(my_module, &my_attr_1.attr);
	error2 = sysfs_create_file(my_module, &my_attr_2.attr);
	error3 = sysfs_create_file(my_module, &my_attr_3.attr);
	if (error1 || error2 || error3)
	{
		pr_alert("Test_1: Failed to create attributes in /sys/\n");
		kobject_put(my_module);
		my_module = NULL;
		return -ENOMEM;
	}
	return 0;
}

static void __exit my_exit(void)
{
	pr_info("Test_1: my_exit()\n");
	sysfs_remove_file(my_module, &my_attr_1.attr);
	sysfs_remove_file(my_module, &my_attr_2.attr);
	sysfs_remove_file(my_module, &my_attr_3.attr);
	kobject_put(my_module);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MKM");
MODULE_DESCRIPTION("Test_1 - floating point without FPU");

//============================================================================//
/**
 * @brief Метод для преобразования строки в число с фикс.точкой.
 *        Используем внутреннее представление (32-бита), формат 20.12
 *        Диапазон и точность чисел ограничены (см.дефайны)
 *        Точность: 0.001
 * @note  TODO: Точность (потеря) фикс.точки в 32-битах (22.10)
 * 
 * @param str - string buffer with number and sign (integer or fp)
 * @return fixedpt - fixed pointer number in 22.10 (24.8) format.
 */
static fixedpt str2fxpt(const char *s)
{
#define isSpace(ch)      ((ch)==' ' || (ch)=='\t')
#define isDigit(ch)      ((ch)>='0' && (ch)<='9')

	fixedpt fp = 0;
	int sign  = 1;
	int num1  = 0;
	int num2  = 0;

	while (isSpace(*s))
	{
		s++;
	}

	if (*s == '-')
	{
		s++;
		sign = -1;
	} else if (*s == '+')
	{
		s++;
		sign = +1;
	}

	while (isDigit(*s))
	{
		num1 = num1 * 10 + (*s++ - '0');
	}

	fp = fixedpt_fromint(num1);

	if (*s == '.')
	{
		fixedpt frac = 0;
		int power = 0;
		int dec_pow[] = { 1, 10, 100, 1000, 10000, 100000 };
		s++;
		while (isDigit(*s))
		{
			num2 = num2 * 10 + (*s++ - '0');
			power++;
			if (power > 4) break;
		}
		frac = (num2 * (1 << FIXEDPT_FBITS)) / dec_pow[power];
		fp = fixedpt_add(fp, frac);
	}

	return (sign * fp);
}

//============================================================================//
/*
Компиляция и установка модуля:
cd test1
make
sudo insmod test1.ko
mkm@mkm-Virtual-Machine:~/my_share/test1$ ls -l /sys/test_1
итого 0
-rw-rw-rw- 1 root root 4096 окт 31 17:08 float_1
-rw-rw-rw- 1 root root 4096 окт 31 17:08 float_2
-r--r--r-- 1 root root 4096 окт 31 17:08 result
cd /sys/test_1
dmesg |tail

:/sys/test_1$ echo 1.41 > float_1
:/sys/test_1$ echo 0.02 > float_2
:/sys/test_1$ cat < result
1.4277
:/sys/test_1$ cat < float_1
1.4082
:/sys/test_1$ cat < float_2
0.0195
:/sys/test_1$ 

sudo rmmod test1.ko


Пример работы программы
# echo 1.41 > float_1
# echo 0.02 > float_2
# cat  float_1
1.41
# cat  float_2
0.02
# cat result
1.43 

*/