#include <iostream>

using std::cerr;
using std::cout;
using std::endl;
using std::string;

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Bad url" << std::endl;
        return 1;
    }
    string url = argv[1];
    cout << url << endl;
    
}
