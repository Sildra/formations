#include "meta.h"
#ifdef JSON_TEST
# include "json.h"
#endif

int main()
{
#ifdef META_TEST
    test_meta();
#endif
#ifdef JSON_TEST
    test_json();
#endif
}