#include "LogoTexts.h"

LogoTextSet Logo(const TextCatalog &texts)
{
    return {{texts.Get("Logo.Start"), texts.Get("Logo.Editor"), texts.Get("Logo.Options"), texts.Get("Logo.Exit")}};
}
