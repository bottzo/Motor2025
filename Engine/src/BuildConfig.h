#pragma once

// Define si estamos en modo desarrollo o release del motor
#ifdef _DEBUG
#define ENGINE_DEVELOPMENT_MODE 1
#define ENGINE_RELEASE_MODE 0
#else
#define ENGINE_DEVELOPMENT_MODE 0
#define ENGINE_RELEASE_MODE 1
#endif