#include "teleoperation_client.h"

int main() {
    TeleoperationClient client("192.168.99.201", "192.168.99.129");
    client.Start();
    return 0;
}
