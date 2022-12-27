#include "../rcconf.h"
#include "../rcconf_list.h"
#include "../rcconf_sublist.h"
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#define CHEAT_NO_MATH
#include <cheat.h>
#include <cheats.h>


CHEAT_DECLARE(
	#define TMP_PATH        "build/tmp"
	#define RCCONF_PATH     TMP_PATH "/test_rc.conf"
	#define RCCONF_OUT_PATH TMP_PATH "/test_rc_out.conf"
	#define INCORRECT_PATH  TMP_PATH "/test_rc_incorrect.conf"
	#define RCCONF_HEADER   "*** HEADER ***"
)

CHEAT_TEST(rcconf,
	struct rcconf cfg;
	struct rcconf_item *item;
	struct rcconf_list *list_item;
	int res;
	FILE *f;
	char buf[64];
	const char *out = RCCONF_HEADER "\nkey1=\"val1\"\nkey2=\"val2\"\nnew2=\"newval2\"\n";

	(void)(cheat_check); // suppress compiler "unused" error

	mkdir(TMP_PATH, 0777);

	f = fopen(RCCONF_PATH, "w");
	fprintf(f, "key1=\"val1\"\n");
	fprintf(f, "key_inv=val\n");
	fprintf(f, "        key2=\"val2\"           \n");
	fprintf(f, "=\"val\"\n");
	fprintf(f, "=\n");
	fprintf(f, "key=\n");
	fprintf(f, "key=\"\n");
	fprintf(f, "\n");
	fprintf(f, "key3=\"val3\"\n");
	fprintf(f, "############\n");
	fclose(f);

	rcconf_init(NULL);
	rcconf_init(&cfg);

	res = rcconf_set_item(&cfg, "key", "val");
	cheat_assert_int(res, 0);
	res = rcconf_set_item(&cfg, NULL, "val");
	cheat_assert_int(res, -EINVAL);
	res = rcconf_set_item(&cfg, "key", NULL);
	cheat_assert_int(res, -EINVAL);

	rcconf_list_foreach(&cfg.list, list_item) {
		item = container_of(list_item, struct rcconf_item, list);
		cheat_assert_string(item->key, "key");
		cheat_assert_string(item->val, "val");
	}

	res = rcconf_set_item(&cfg, "key", "VAL");
	item = container_of(cfg.list.next, struct rcconf_item, list);
	cheat_assert_string(item->val, "VAL");

	item = rcconf_get_item(&cfg, NULL);
	cheat_assert_pointer(item, NULL);

	item = rcconf_get_item(NULL, "key");
	cheat_assert_pointer(item, NULL);

	item = rcconf_get_item(&cfg, "key");
	cheat_assert_not_pointer(item, NULL);
	cheat_assert_string(item->val, "VAL");

	res = rcconf_add_item(&cfg, NULL);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_add_item(NULL, item);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_add_item(&cfg, item);
	cheat_assert_int(res, -EEXIST);

	item = rcconf_make_item(NULL, "VAL");
	cheat_assert_pointer(item, NULL);
	item = rcconf_make_item("key", NULL);
	cheat_assert_pointer(item, NULL);
	item = rcconf_make_item("key2", "val2");
	cheat_assert_not_pointer(item, NULL);

	res = rcconf_load(NULL, RCCONF_PATH);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_load(&cfg, NULL);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_load(&cfg, INCORRECT_PATH);
	cheat_assert_int(res, -ENOENT);
	res = rcconf_load(&cfg, RCCONF_PATH);
	cheat_assert_int(res, 0);

	item = rcconf_get_item(&cfg, "key");
	cheat_assert_pointer(item, NULL);
	item = rcconf_get_item(&cfg, "key1");
	cheat_assert_not_pointer(item, NULL);

	res = rcconf_del_item(NULL, "key3");
	cheat_assert_int(res, -EINVAL);
	res = rcconf_del_item(&cfg, NULL);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_del_item(&cfg, "unknown");
	cheat_assert_int(res, -ENOENT);
	res = rcconf_del_item(&cfg, "key3");
	cheat_assert_int(res, 0);

	res = rcconf_set_item(&cfg, "new", "newval");
	cheat_assert_int(res, 0);

	res = rcconf_save(NULL, RCCONF_OUT_PATH, RCCONF_HEADER);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_save(&cfg, NULL, RCCONF_HEADER);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_save(&cfg, TMP_PATH, RCCONF_HEADER);
	cheat_assert_int(res, -EISDIR);
	res = rcconf_save(&cfg, RCCONF_OUT_PATH, RCCONF_HEADER);
	cheat_assert_int(res, 0);

	rcconf_free_item(NULL);

	res = rcconf_save_items(NULL, NULL, NULL);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_save_items(INCORRECT_PATH, NULL, NULL);
	cheat_assert_int(res, -ENOENT);

	rcconf_save_items(RCCONF_OUT_PATH, RCCONF_HEADER, "new", NULL, "new2", "newval2", NULL);

	f = fopen(RCCONF_OUT_PATH, "r");
	fread(buf, sizeof(buf), 1, f);
	fclose(f);

	cheat_assert_int(strcmp(buf, out), 0);

	rcconf_free(&cfg);
	rcconf_free(NULL);
)

CHEAT_TEST(rcconf_sublist,
	struct rcconf_sublist sublist;
	struct rcconf_sublist_item *item;
	struct rcconf_list *list_item;
	struct rcconf cfg;
	struct rcconf_item *rcconf_item;
	int res;

	rcconf_sublist_init(NULL, "listkey", "prefix", "key");
	rcconf_sublist_init(&sublist, NULL, "prefix", "key");
	rcconf_sublist_init(&sublist, "listkey", NULL, "key");
	rcconf_sublist_init(&sublist, "listkey", "prefix", NULL);

	rcconf_sublist_init(&sublist, "listkey", "prefix", "key");
	cheat_assert_int(sublist.size, 0);

	rcconf_sublist_append(NULL, "value1");
	rcconf_sublist_append(&sublist, "value1");
	cheat_assert_int(sublist.size, 1);

	rcconf_sublist_foreach(&sublist, item) {
		cheat_assert_string(item->val, "value1");
	}

	rcconf_sublist_append(&sublist, "value2");
	cheat_assert_int(sublist.size, 2);
	item = rcconf_sublist_first(&sublist);
	cheat_assert_string(item->val, "value1");
	list_item = sublist.list.prev;
	item = container_of(list_item, struct rcconf_sublist_item, list);
	cheat_assert_string(item->val, "value2");

	rcconf_sublist_del_item(NULL, item);
	rcconf_sublist_del_item(&sublist, NULL);

	rcconf_sublist_del_item(&sublist, item);
	cheat_assert_int(sublist.size, 1);
	list_item = sublist.list.prev;
	item = container_of(list_item, struct rcconf_sublist_item, list);
	cheat_assert_string(item->val, "value1");

	res = rcconf_sublist_del_item_by_val(&sublist, NULL);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_sublist_del_item_by_val(&sublist, "unknownvalue");
	cheat_assert_int(res, -ENOENT);
	rcconf_sublist_append(&sublist, "value3");
	rcconf_sublist_del_item_by_val(&sublist, "value1");
	item = rcconf_sublist_first(&sublist);
	cheat_assert_string(item->val, "value3");

	res = rcconf_sublist_set(NULL, "val");
	cheat_assert_int(res, -EINVAL);
	res = rcconf_sublist_set(&sublist, NULL);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_sublist_set(&sublist, "value4");
	cheat_assert_int(res, 0);
	res = rcconf_sublist_set(&sublist, "value3");
	cheat_assert_int(res, 0);
	item = rcconf_sublist_first(&sublist);
	cheat_assert_string(item->val, "value4");

	rcconf_sublist_free(NULL);
	rcconf_sublist_free(&sublist);
	cheat_assert_int(sublist.size, 0);

	rcconf_sublist_item_free(NULL);

	rcconf_init(&cfg);
	rcconf_sublist_init(&sublist, "listkey", "prefix_", "key");

	res = rcconf_sublist_load(NULL, &cfg);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_sublist_load(&sublist, NULL);
	cheat_assert_int(res, -EINVAL);

	rcconf_set_item(&cfg, "listkey", "");
	res = rcconf_sublist_load(&sublist, &cfg);
	cheat_assert_int(res, 0);
	cheat_assert_int(sublist.size, 0);

	rcconf_set_item(&cfg, "listkey", "nokey0");
	res = rcconf_sublist_load(&sublist, &cfg);
	cheat_assert_int(res, -ENOENT);

	rcconf_set_item(&cfg, "listkey", "   key0 key1   key2   ");
	rcconf_set_item(&cfg, "prefix_key0", "val0");
	rcconf_set_item(&cfg, "prefix_key1", "val1");
	rcconf_set_item(&cfg, "prefix_key2", "val2");

	rcconf_item = rcconf_get_item(&cfg, "prefix_key0");
	cheat_assert_not_pointer(rcconf_item, NULL);

	res = rcconf_sublist_load(&sublist, &cfg);
	cheat_assert_int(res, 0);
	cheat_assert_int(sublist.size, 3);

	rcconf_item = rcconf_get_item(&cfg, "prefix_key0");
	cheat_assert_pointer(rcconf_item, NULL);

	rcconf_free(&cfg);

	res = rcconf_sublist_save(NULL, &cfg);
	cheat_assert_int(res, -EINVAL);
	res = rcconf_sublist_save(&sublist, NULL);
	cheat_assert_int(res, -EINVAL);

	res = rcconf_sublist_save(&sublist, &cfg);
	cheat_assert_int(res, 0);

	rcconf_item = rcconf_get_item(&cfg, "listkey");
	cheat_assert_string(rcconf_item->val, "key0 key1 key2");
	rcconf_item = rcconf_get_item(&cfg, "prefix_key0");
	cheat_assert_string(rcconf_item->val, "val0");
	rcconf_item = rcconf_get_item(&cfg, "prefix_key2");
	cheat_assert_string(rcconf_item->val, "val2");

	rcconf_sublist_free(&sublist);
)
