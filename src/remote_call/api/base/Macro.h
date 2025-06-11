#pragma once

#ifndef REMOTE_CALL_API
#ifdef REMOTE_CALL_EXPORT
#define REMOTE_CALL_API [[maybe_unused]] __declspec(dllexport)
#else
#define REMOTE_CALL_API [[maybe_unused]] __declspec(dllimport)
#endif
#endif
