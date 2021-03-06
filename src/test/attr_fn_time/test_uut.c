#include "license_pbs.h" /* See here for the software license */
#include "test_attr_fn_time.h"
#include <stdlib.h>
#include <stdio.h>

#include "attribute.h"
#include "pbs_error.h"

int get_time_string(char *time_string, int string_size, long timeval);

START_TEST(test_get_time_string)
  {
  char buf[1024];
  get_time_string(buf, sizeof(buf), 200);
  fail_unless(!strcmp(buf, "00:03:20"));

  get_time_string(buf, sizeof(buf), 3800);
  fail_unless(!strcmp(buf, "01:03:20"));

  get_time_string(buf, sizeof(buf), 7523);
  fail_unless(!strcmp(buf, "02:05:23"), "time: %s", buf);

  get_time_string(buf, sizeof(buf), 380);
  fail_unless(!strcmp(buf, "00:06:20"));
  }
END_TEST

START_TEST(test_two)
  {


  }
END_TEST

Suite *attr_fn_time_suite(void)
  {
  Suite *s = suite_create("attr_fn_time_suite methods");
  TCase *tc_core = tcase_create("test_get_time_string");
  tcase_add_test(tc_core, test_get_time_string);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("test_two");
  tcase_add_test(tc_core, test_two);
  suite_add_tcase(s, tc_core);

  return s;
  }

void rundebug()
  {
  }

int main(void)
  {
  int number_failed = 0;
  SRunner *sr = NULL;
  rundebug();
  sr = srunner_create(attr_fn_time_suite());
  srunner_set_log(sr, "attr_fn_time_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return number_failed;
  }
