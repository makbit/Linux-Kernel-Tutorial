/**
 * @file test3.c
 * @brief Модуль ядра, который создаёт иерархию имён в /proc глубиной
 *        больше или равной 2 (задавайте путь строкой в параметре загрузки),
 *        чтобы в это имя можно было писать целочисленное значение,
 *        и считывать его оттуда.
 *        Запись не численного значения должна возвращать ошибку и
 *        не изменять ранее записанное значение.
 * @note  Создаёт атрибут по адресу: /proc/test3/sub01/sub02.../depth
 *
 * @date 2023-11-04
 * @author K.M.Maximov
 */
//============================================================================//
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/version.h>

const char *szDeviceName = "test3";
const char *szValueName = "Depth";
static uint Depth = 2;
module_param(Depth, uint, S_IRUGO);
MODULE_PARM_DESC(Depth, "Depth value (1..9):");

static struct MyProcessData
{
	uint                  nDepth;
	struct proc_dir_entry *pRoot;
	struct proc_dir_entry *aDir[10];
} my_proc;

static ssize_t my_read(struct file *f, char __user *buf, size_t length, loff_t *offset);
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *offset);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	static const struct proc_ops my_ops = {
		.proc_read = my_read,
		.proc_write = my_write,
	};
#else
	static const struct file_operations my_ops = {
		.read = my_read,
		.write = my_write,
	};
#endif

//============================================================================//
static ssize_t my_read(struct file *f, char __user *buf, size_t length, loff_t *offset)
{
	pr_info("Test_3: my_read([%lu] bytes, [%lld] offset).\n", length, *offset);
	if (*offset)
	{
		return 0;
	}
	else
	{
		// TODO: we use _string_ (not binary) output.
		*offset = sprintf(buf, "%u", Depth);
		return (*offset);
	}
}

//============================================================================//
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *offset)
{
	#define isDigit(ch)     ((ch) >= '0' && (ch) <= '9')
	pr_info("Test_2: my_write([%lu] bytes, [%lld] offset).\n", len, *offset);
	if (isDigit(buf[0]))
	{
		uint val = 0;
		//kstrtoul_from_user(buf, len, 10, &val);
		if (sscanf(buf, "%9u", &val) == 1)
		{
			Depth = val;
			*offset = len; // too much?
			return len;
		}
	}
	return -EINVAL;
}

//============================================================================//
/**
 * @brief Метод для инициализации модуля ядра и создания атрибута в /proc
 * @note  Метод строит дерево вида: /proc/test3/sub01/sub02.../Depth
 * 
 * @return int - возвращает результат работы, 0 - успех.
 */
static int __init my_init(void)
{
	uint i;
	char subDirName[8] = "sub00";

	if (Depth > 9)
	{
		Depth = 9;
	}
	else if (Depth < 2)
	{
		Depth = 2;
	}

	pr_info("Test_3: my_init(), Depth=%u\n", Depth);

	my_proc.nDepth = Depth;
	my_proc.pRoot = proc_mkdir(szDeviceName, NULL);
	if (my_proc.pRoot == NULL)
	{
		pr_alert("Test_3: failed to create root folder in /proc/\n");
		return -ENOMEM;
	}

	my_proc.aDir[0] = my_proc.pRoot;

	for (i = 1; i < my_proc.nDepth; i++)
	{
		subDirName[4] = '0' + i;
		my_proc.aDir[i] = proc_mkdir(subDirName, my_proc.aDir[i-1]);
		if (my_proc.aDir[i] == NULL)
		{
			pr_alert("Test_3: failed to create subfolder [%s] in /proc/test3\n", subDirName);
			remove_proc_subtree(szDeviceName, NULL);
			return -ENOMEM;
		}
	}

	//NOTE: i == my_proc.nDepth
	my_proc.aDir[i] = proc_create(szValueName, 0666, my_proc.aDir[i-1], &my_ops);
	if (my_proc.aDir[i] == NULL)
	{
		pr_alert("Test_3: failed to create attribute in /proc/test3\n");
		remove_proc_subtree(szDeviceName, NULL);
		return -ENOMEM;
	}

	return 0;
}

//============================================================================//
static void __exit my_exit(void)
{
	pr_info("Test_3: my_exit()\n");
	remove_proc_subtree(szDeviceName, NULL);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MKM");
MODULE_DESCRIPTION("Test_3 - module params testing.");
//============================================================================//
