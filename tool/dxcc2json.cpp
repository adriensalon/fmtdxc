#include <fmtdxc/fmtdxc.hpp>

#include <fstream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: drag a .dxcc file onto the executable\n";
        return 1;
    }
    std::filesystem::path _input_path(argv[1]);
    if (!std::filesystem::exists(_input_path)) {
        std::cerr << "Error: File does not exist\n";
        return 2;
    }
    if (_input_path.extension() != ".dxcc") {
        std::cerr << "Error: File is not an dawxchange project container\n";
        return 3;
    }
    std::ifstream _input_stream(_input_path, std::ios::binary);
    
    // TODO

    return 0;
}