/**
 * @file  test2.c
 * @brief Тестовое задание 2
 *        Модуль ядра, драйвер символьного устройства /dev/chardev, который
 *        пишет в буфер строки, сортирует их и при чтении выводит по одной
 *        в алфавитном порядке.
 *        Строка которая была прочитана должна быть удалена из буфера.
 *        Если в буфере нет строк то устройство при чтении должно вывести 0.
 * @note  Для теста длина строк ограничена.
 *
 * @date 2023-10-31
 * @author K.M.Maximov
 */
//============================================================================//
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/list_sort.h>

const char *DEVICE_NAME = "chardev";
const char *CLASS_NAME = "chardev_class";
static dev_t device_number = MKDEV(0, 0);
static struct class *device_class = NULL;
static struct cdev c_dev;

static ssize_t my_read(struct file *f, char __user *buf, size_t n, loff_t *pos);
static ssize_t my_write(struct file *f, const char __user *, size_t, loff_t *);

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read  = my_read,
	.write = my_write,
};

static LIST_HEAD(my_list);

struct my_list_node
{
	struct list_head list;
	size_t size;
	char   str[1];
};

//============================================================================//
/**
 * @brief Функция сравнения двух элементов (строк) списка - a, b.
 * 
 * @param priv - не используется
 * @param a - указатель на 1-ый элемент
 * @param b - указатель на 2-ой элемент
 * @return int - результат сравнения (-1, 0, 1).
 */
int my_compare(void* priv, const struct list_head *a, const struct list_head *b)
{
	const struct my_list_node *node1 = container_of(a, struct my_list_node, list);
	const struct my_list_node *node2 = container_of(b, struct my_list_node, list);
	return strcmp(node1->str, node2->str);
}

void print_my_list(void)
{
	struct list_head *pos;
	struct my_list_node *node;
	pr_info("Test_2: LIST: ");
	list_for_each(pos, &my_list)
	{
		node = list_entry(pos, struct my_list_node, list);
		pr_info("(%s) ", node->str);
	}
}

//============================================================================//
/**
 * @brief Метод для загрузки модуля, поиска свободного номера, создания
 *        класса устройства и самого устройства /sys/chardev/.
 * @note  Рекумендуется после создания изменить chmod 0666 /sys/chardev
 */
static int __init my_init(void)
{
	int error;
	struct device *dev = NULL;
	pr_info("Test_2: my_init()\n");

	error = alloc_chrdev_region(&device_number, 0, 1, DEVICE_NAME);
	if (error)
	{
		pr_alert("Test_2: Failed to allocate device_number!\n");
		return error;
	}

	device_class = class_create(THIS_MODULE, CLASS_NAME);
	if (device_class == NULL)
	{
		pr_alert("Test_2: Failed to create device_class [%s]!\n", CLASS_NAME);
		unregister_chrdev_region(device_number, 1);
		return error;
	}

	dev = device_create(device_class, NULL, device_number, NULL, DEVICE_NAME);
	if (dev == NULL)
	{
		pr_alert("Test_2: Failed to create device [%s]!\n", DEVICE_NAME);
		class_destroy(device_class);
		unregister_chrdev_region(device_number, 1);
		return -1;
	}

	cdev_init(&c_dev, &fops);
	error = cdev_add(&c_dev, device_number, 1);
	if (error)
	{
		pr_alert("Test_2: CDev registration failed!\n");
		device_destroy(device_class, device_number);
		class_destroy(device_class);
		unregister_chrdev_region(device_number, 1);
		return error;
	}

	return 0;
}

//============================================================================//
/**
 * @brief Метод завершения работы модуля и освобождения ресурсов/памяти.
 * @note  При работе модуля внутри ядра не вызывается.
 */
static void __exit my_exit(void)
{
	struct list_head *pos, *temp;
	struct my_list_node *node;
	pr_info("Test_2: my_exit()\n");

	device_destroy(device_class, device_number);
	class_destroy(device_class);
	cdev_del(&c_dev);
	unregister_chrdev_region(device_number, 1);

	list_for_each_safe(pos, temp, &my_list)
	{
		node = list_entry(pos, struct my_list_node, list);
		if (node)
		{
			list_del(&node->list);
			kfree(node);
		}
	}
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MKM");
MODULE_DESCRIPTION("Test_2 - character device with a list of strings.");

//============================================================================//
/**
 * @brief Метод для записи данных от пользователя в наш драйвер,
 *        помещает строку в наш список строк.
 * 
 * @param f - указатель на файл (не используется)
 * @param buf - пользовательский буфер с данными (user-space)
 * @param len - количество данных в буфере для записи
 * @param offset - смещение указателя в буфере данных (не используем)
 * @return ssize_t - кол-во переданных/принятых байт.
 */
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *offset)
{
	struct my_list_node *node;
	int mem_size = len + sizeof(struct my_list_node);

	pr_info("Test_2: my_write([%lu] bytes, [%lld] offset).\n", len, *offset);

	if (*offset)
	{
		pr_info("Test_2: my_write() - offset not supported.\n" );
		return -EINVAL;
	}

	node = (struct my_list_node *) kmalloc(mem_size, GFP_KERNEL);
	if (node == NULL)
	{
		return -ENOMEM;
	}

	node->size = len;
	if (copy_from_user(node->str, buf, len))
	{
		kfree(node);
		return -EFAULT;
	}

	list_add(&node->list, &my_list);
	list_sort(NULL, &my_list, my_compare);

	return len;
}

//============================================================================//
static ssize_t my_read(struct file *f, char __user *buf, size_t length, loff_t *offset)
{
	pr_info("Test_2: my_read([%lu] bytes, [%lld] offset).\n", length, *offset);

	if (*offset)
	{
		// end of file
		(*offset) = 0;
		return 0;
	}
	else
	{
		if (list_empty(&my_list))
		{
			pr_info("Test_2: my_read() List is empty.\n" );
			return ((*offset) += sprintf(buf, "%d\n", 0));
		}
		else
		{
			struct my_list_node *node;

			node = list_entry(my_list.next, struct my_list_node, list);

			length = (node->size < length) ? node->size : length;
			if (copy_to_user(buf, node->str, length))
			{
				(*offset) = 0;
				return -EFAULT;
			}

			list_del(&node->list);
			kfree(node);

			(*offset) = length;
			return length;
		}
	}
}

//============================================================================//
/*
sudo insmod test2.ko
mkm@:~/my_share/test2$ sudo chmod 0666 /dev/chardev
mkm@:~/my_share/test2$ ls -al /dev/chardev

Пример:  
#echo cat> /dev/chardev
#echo xyz > /dev/chardev
#echo dog > /dev/chardev
#cat  /dev/chardev
cat
#cat  /dev/chardev
dog 
#cat  /dev/chardev
xyz
#cat  /dev/chardev
0
*/
