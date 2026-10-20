#include <Shogi.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cout << "エンジンパスを２つ指定してください。" << std::endl;
        return 0;
    }

    for (int i = 0; i < argc; i++)
    {
        // 引数を表示
        std::cout << "argv[" << i << "] = " << argv[i] << std::endl;
    }
    return 0;
}