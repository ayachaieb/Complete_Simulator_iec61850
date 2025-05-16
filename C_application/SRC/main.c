#include "common/assert_handler.h"
#include "common/trace.h"
#include "app/drive.h"
#include "app/enemy.h"
#include "app/line.h"
#include "app/state_machine.h"

int main(void)
{
    // Initialize the system
    state_machine_run();

    ASSERT(0);
    return 0;
}