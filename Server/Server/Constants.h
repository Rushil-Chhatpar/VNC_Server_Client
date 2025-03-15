#pragma once

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

const int BYTES_PER_PIXEL = 4;

const int PORT = 5900;
const int CLIENT_UDP_PORT = 55556;
const int UDP_PORT = 55555;

// As mentioned in https://www.erg.abdn.ac.uk/users/gorry/course/inet-pages/udp.html#:~:text=A%20UDP%20datagram%20is%20carried,packets%20usually%20requires%20IP%20fragmentation.
// max size is 65507, but i will set it to 20000 to leave some room for any headers and to be on the safe side
const size_t MAX_UDP_PAYLOAD = 20000;

const int BUTTON_MASK_HASH_PRESS = 91;
const int BUTTON_MASK_HASH_RELEASE = 92;

// for 256x256 pixel checking
const int BLOCK_CHECKING_SIZE = 256;