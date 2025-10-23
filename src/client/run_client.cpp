#include "teleoperation_client.h"

int main() {
    TeleoperationClient client("localhost", "localhost");
    client.Start();
    return 0;
}
