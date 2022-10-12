#ifndef DRIVERS_INIT_H
#define DRIVERS_INIT_H

#include <memory>

/// External hardware drivers
namespace Drivers {
namespace Display {
class Base;
}

void Init();
void CleanUp();

std::shared_ptr<Display::Base> &GetDisplayDriver();
}

#endif
