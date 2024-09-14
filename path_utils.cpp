#include "path_utils.h"

#include <unistd.h>

String get_executable_path()
{
    String result = talloc_string(PATH_MAX);
    memset(result.data, 0, sizeof(PATH_MAX));

    auto size_wrote = readlink("/proc/self/exe", (char*)result.data, PATH_MAX);

    if (size_wrote == -1)
    {
        fprintf(stderr, "Couldn't get the path to running exe...\n");
        exit(1);
    }

    result.count = size_wrote;

    return result;
}

void setcwd(String dir)
{
    if (dir)
    {
        char *c_string_dir = (char*)temp_c_string(dir);
        // printf("cd to '%s'\n", c_string_dir);
        i32 success = chdir(c_string_dir);
        if (success == -1)
        {
            fprintf(stderr, "Cannot cd to '%s'...\n", c_string_dir);
            assert(0);
        }
        reset_temporary_storage();
    }
    else
    {
        fprintf(stderr, "The directory of the executable is NULL. Cannot chdir() there...\n");
        assert(0);
    }
}
