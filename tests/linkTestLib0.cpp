#include <Vc/Vc>
#include <Vc/IO>

using namespace Vc;
float_v fooLib0(float_v a)
{
    a += float_v::One();
    std::cerr << a;
    return a;
}
