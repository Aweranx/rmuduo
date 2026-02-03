#include "Timestamp.h"
#include <iostream>
using namespace rmuduo;
int main() {
  std::cout << Timestamp::now().toString() << std::endl;
  std::cout << Timestamp::now().toFormattedString() << std::endl;
}