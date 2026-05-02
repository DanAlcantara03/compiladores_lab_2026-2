#include "regex.h"
#include "nfa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void print_postfix(regex r)
{
    for (size_t i = 0; i < r.size; i++)
    {
        printf("%c", r.items[i]);
    }
    printf("\n");
}

void test_strings_stdin(const char *regex_str)
{
    regex infix = create_regex(regex_str);
    regex r = parse_regex(&infix);
    nfa n = regex_to_nfa(r);
    regex_free(&r);
    regex_free(&infix);

    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin))
    {
        buf[strcspn(buf, "\r\n")] = '\0';
        int result = match_nfa(n, buf, strlen(buf));
        printf("%d", result ? 1 : 0);
    }
    printf("\n");
    nfa_free(&n);
}

int main(int argc, char *argv[])
{
    int opt;
    char regex_str[1024];

    while ((opt = getopt(argc, argv, "rt")) != -1)
    {
        switch (opt)
        {
            case 'r':
                if (!fgets(regex_str, sizeof(regex_str), stdin))
                    return 1;
                regex_str[strcspn(regex_str, "\r\n")] = '\0';
                {
                    regex infix = create_regex(regex_str);
                    regex postfix = parse_regex(&infix);
                    print_postfix(postfix);
                    regex_free(&postfix);
                    regex_free(&infix);
                }
                return 0;
            case 't':
                if (!fgets(regex_str, sizeof(regex_str), stdin))
                    return 1;
                regex_str[strcspn(regex_str, "\r\n")] = '\0';
                test_strings_stdin(regex_str);
                return 0;
            default:
                fprintf(stderr, "Usage: %s -r | -t\n", argv[0]);
                return 1;
        }
    }

    fprintf(stderr, "Usage: %s -r | -t\n", argv[0]);
    return 1;
}