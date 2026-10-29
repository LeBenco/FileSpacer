// Loads the application's own resources for regional language variants and
// checks that each primary language resolves to its translation, as the
// resource loader does for the user's display language with en-US fallback.
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include "resource.h"

namespace {
struct Case { const wchar_t *language; const wchar_t *expected; };
const Case cases[] = {
    {L"en-US", L"Quick access"}, {L"en-GB", L"Quick access"}, {L"nl-NL", L"Quick access"},
    {L"fr-FR", L"Accès rapide"}, {L"fr-CA", L"Accès rapide"}, {L"fr-BE", L"Accès rapide"},
    {L"de-DE", L"Schnellzugriff"}, {L"de-AT", L"Schnellzugriff"}, {L"de-CH", L"Schnellzugriff"},
    {L"it-IT", L"Accesso rapido"}, {L"it-CH", L"Accesso rapido"},
    {L"es-ES", L"Acceso rápido"}, {L"es-MX", L"Acceso rápido"}, {L"es-AR", L"Acceso rápido"},
    {L"pt-PT", L"Acesso rápido"}, {L"pt-BR", L"Acesso rápido"},
    {L"el-GR", L"Γρήγορη πρόσβαση"},
};
}

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) { fwprintf(stderr, L"usage: LanguageTests <FileSpacer.exe>\n"); return 2; }
    HMODULE module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module) { fwprintf(stderr, L"FAIL: cannot load %s (error %lu)\n", argv[1], GetLastError()); return 1; }
    int failures = 0;
    for (const Case &entry : cases) {
        // Same list as wWinMain: the display language, then en-US, double terminated.
        std::wstring list = entry.language;
        list.push_back(L'\0'); list.append(L"en-US", 6); list.push_back(L'\0');
        ULONG count = 0;
        if (!SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, list.c_str(), &count)) {
            fwprintf(stderr, L"FAIL: SetThreadPreferredUILanguages(%s) error %lu\n", entry.language, GetLastError());
            return 1;
        }
        wchar_t text[128] = {};
        const int length = LoadStringW(module, IDS_QUICK_ACCESS, text, 128);
        const bool ok = length > 0 && wcscmp(text, entry.expected) == 0;
        wprintf(L"%s %-6s IDS_QUICK_ACCESS=\"%s\" expected \"%s\"\n", ok ? L"PASS" : L"FAIL", entry.language, text, entry.expected);
        if (!ok) ++failures;
    }
    FreeLibrary(module);
    return failures ? 1 : 0;
}
