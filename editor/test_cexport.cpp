#include "c_export.h"
#include <QRegularExpression>
#include <cstdio>

static int g_checks = 0, g_fail = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; \
std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

    // Reads the bytes back out of the generated text, the way a C compiler would see them.
    static QByteArray parseBack(const QString &h)
    {
        QByteArray out;
        const int open = h.indexOf("= {");
        if (open < 0) return out;
        QRegularExpressionMatchIterator it =
            QRegularExpression("0x([0-9A-F]{2})").globalMatch(h.mid(open));
        while (it.hasNext())
            out.append(static_cast<char>(it.next().captured(1).toUInt(nullptr, 16)));
        return out;
    }

int main()
{
    const uint8_t raw[] = {0x4C, 0x44, 0x01, 0x00, 0x02, 0x00, 0xAB, 0xCD,
                           0x00, 0x00, 0x00, 0x00, 0xFF};
    const QByteArray img(reinterpret_cast<const char *>(raw), sizeof raw);

    const QString h = imageToCHeader(img, "demo.json");
    CHECK(h.startsWith("/*"));
    CHECK(h.contains("#define LADDER_PROGRAM_SIZE 13"));
    CHECK(h.contains("static const uint8_t ladder_program[LADDER_PROGRAM_SIZE] = {"));
    CHECK(h.contains("0x4C, 0x44, 0x01, 0x00, 0x02, 0x00, 0xAB, 0xCD, 0x00, 0x00, 0x00, 0x00,\n    0xFF\n};"));
    CHECK(parseBack(h) == img);                    // nothing lost, nothing added
    CHECK(h.contains("demo.json"));

    CHECK(imageToCHeader(QByteArray(), "x").isEmpty());

    const QString h2 = imageToCHeader(img, "evil*/name");
    CHECK(!h2.contains("evil*/name"));              // the file name was neutralized
    CHECK(h2.count("/*") == h2.count("*/"));        // every comment that opens also closes
    CHECK(parseBack(h2) == img);

    std::printf("%d checks, %d failures\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}