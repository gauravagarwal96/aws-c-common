/*
 * Copyright 2010-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <aws/common/log_writer.h>

#include <aws/common/string.h>
#include <aws/testing/aws_test_harness.h>

#include <errno.h>
#include <stdio.h>

#ifndef WIN32
#    include <sys/file.h>
#endif /* WIN32 */

#ifdef _MSC_VER
#    pragma warning(disable : 4996) /* Disable warnings about fopen() being insecure */
#endif                              /* _MSC_VER */

#define TEST_WRITER_MAX_BUFFER_SIZE 4096

/*
 * We need to add some filesystem support in common.  Hard-code for now.
 */
static const char *s_test_file_name =
#ifdef WIN32
    "aws_log_writer_test.log";
#else
    "./aws_log_writer_test.log";
#endif

int do_default_log_writer_test(
    struct aws_log_writer *writer,
    const char *expected_file_content,
    const struct aws_string *output,
    FILE *close_fp) {

    int result = writer->vtable->write(writer, output);

    aws_log_writer_clean_up(writer);

    /*
     * When we redirect stdout/stderr to a file, we need to close the file manually since the writer implementations do
     * not do so.
     */
    if (close_fp != NULL) {
        fclose(close_fp);
    }

    char buffer[TEST_WRITER_MAX_BUFFER_SIZE];
    FILE *file = fopen(s_test_file_name, "r");
    int open_error = errno;
    size_t bytes_read = 0;

    if (file != NULL) {
        bytes_read = fread(buffer, 1, TEST_WRITER_MAX_BUFFER_SIZE - 1, file);
        fclose(file);
    }
    remove(s_test_file_name);

    /*
     * Check that the write call was successful
     */
    ASSERT_TRUE(result == AWS_OP_SUCCESS, "Writing operation failed");

    /*
     * Check the file was read successfully
     */
    ASSERT_TRUE(
        file != NULL, "Unable to open output file \"%s\" to verify contents. Error: %d", s_test_file_name, open_error);
    ASSERT_TRUE(bytes_read >= 0, "Failed to read test output file \"%s\"", s_test_file_name);

    /*
     * add end of string marker
     */
    buffer[bytes_read] = 0;

    /*
     * Check file contents
     */
    ASSERT_TRUE(
        strcmp(buffer, expected_file_content) == 0,
        "Expected log file to contain:\n\n%s\n\nbut instead it contained:\n\n%s\n",
        expected_file_content,
        buffer);

    return AWS_OP_SUCCESS;
}

/*
 * Test cases
 */

#define EXISTING_TEXT "Some existing text\n"
#define SIMPLE_FILE_CONTENT "A few\nlog lines.\n"
static const char *s_combined_text = EXISTING_TEXT SIMPLE_FILE_CONTENT;

AWS_STATIC_STRING_FROM_LITERAL(s_simple_file_content, SIMPLE_FILE_CONTENT);

/*
 * Simple file test
 */
static int s_log_writer_simple_file_test(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;

    remove(s_test_file_name);

    struct aws_log_writer_file_options options = {.filename = s_test_file_name};

    struct aws_log_writer writer;
    aws_log_writer_init_file(&writer, allocator, &options);

    return do_default_log_writer_test(&writer, SIMPLE_FILE_CONTENT, s_simple_file_content, NULL);
}
AWS_TEST_CASE(test_log_writer_simple_file_test, s_log_writer_simple_file_test);

/*
 * Existing file test (verifies append is being used)
 */
static int s_log_writer_existing_file_test(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;

    remove(s_test_file_name);
    FILE *fp = fopen(s_test_file_name, "w+");
    fprintf(fp, EXISTING_TEXT);
    fclose(fp);

    struct aws_log_writer_file_options options = {.filename = s_test_file_name};

    struct aws_log_writer writer;
    aws_log_writer_init_file(&writer, allocator, &options);

    return do_default_log_writer_test(&writer, s_combined_text, s_simple_file_content, NULL);
}
AWS_TEST_CASE(test_log_writer_existing_file_test, s_log_writer_existing_file_test);

/*
 * (Error case) Bad filename test
 */
static int s_log_writer_bad_file_test(struct aws_allocator *allocator, void *ctx) {
    (void)ctx;

    struct aws_log_writer_file_options options = {.filename = "."};

    struct aws_log_writer writer;
    int result = aws_log_writer_init_file(&writer, allocator, &options);
    int aws_error = aws_last_error();

    ASSERT_TRUE(result == AWS_OP_ERR, "Log file open succeeded despite an invalid file name");

#ifdef WIN32
    ASSERT_TRUE(aws_error == AWS_ERROR_NO_PERMISSION, "File open error was not no permission as expected");
#else
    ASSERT_TRUE(aws_error == AWS_ERROR_FILE_INVALID_PATH, "File open error was not invalid path as expected");
#endif /* WIN32 */

    return AWS_OP_SUCCESS;
}
AWS_TEST_CASE(test_log_writer_bad_file_test, s_log_writer_bad_file_test);
