/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

// gfx::Image and gfx::Rgb replace QImage, QRgb and QColor. While Qt is linked (until step Q5 of
// the iPad port) they are compared with them here: every operation the game uses on random
// images, lighter() and darker() on every colour, and PNG both ways; the smooth scaling, which
// works differently from Qt's, on its own. With the game data's
// directory as the argument, the JPEG images of the 3D cockpits are compared too; their decoders
// differ, so that only reports how much.

#include "gfx/image.h"

#include <QBuffer>
#include <QColor>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>

#include <cstring>
#include <iostream>
#include <map>
#include <random>


namespace {

int g_checks = 0;
int g_failures = 0;
std::mt19937 g_random(1);


std::map<std::string, int> g_kinds;

void fail(const std::string &what)
{
    // the first few of each kind
    const std::string kind = what.substr(0, what.find_first_of("(: "));
    if (++g_kinds[kind] <= 5)
        std::cerr << "FAIL " << what << std::endl;
    g_failures++;
}


void check(bool ok, const std::string &what)
{
    g_checks++;
    if (!ok)
        fail(what);
}


int random(int max)
{
    return std::uniform_int_distribution<int>(0, max)(g_random);
}


QImage::Format qtFormat(gfx::Image::Format format)
{
    switch (format) {
    case gfx::Image::Indexed8: return QImage::Format_Indexed8;
    case gfx::Image::RGB888: return QImage::Format_RGB888;
    case gfx::Image::ARGB32: return QImage::Format_ARGB32;
    case gfx::Image::RGBA8888: return QImage::Format_RGBA8888;
    case gfx::Image::Invalid: break;
    }
    return QImage::Format_Invalid;
}


const char *name(gfx::Image::Format format)
{
    switch (format) {
    case gfx::Image::Indexed8: return "Indexed8";
    case gfx::Image::RGB888: return "RGB888";
    case gfx::Image::ARGB32: return "ARGB32";
    case gfx::Image::RGBA8888: return "RGBA8888";
    case gfx::Image::Invalid: break;
    }
    return "Invalid";
}


// the same pixels, rows and colour table
void same(const std::string &what, const QImage &expected, const gfx::Image &actual)
{
    g_checks++;
    if (expected.isNull() != actual.isNull() || expected.width() != actual.width() || expected.height() != actual.height()
            || qtFormat(actual.format()) != expected.format()) {
        fail(what + ": " + std::to_string(expected.width()) + "x" + std::to_string(expected.height()) + " format " + std::to_string(expected.format())
             + " in Qt, ours " + std::to_string(actual.width()) + "x" + std::to_string(actual.height()) + " " + name(actual.format()));
        return;
    }
    if (expected.isNull())
        return;
    for (int y = 0; y < actual.height(); y++) {
        if (std::memcmp(expected.constScanLine(y), actual.constScanLine(y), actual.bytesPerLine()) != 0) {
            for (int x = 0; x < actual.width(); x++)
                if (expected.pixel(x, y) != actual.pixel(x, y)) {
                    char text[100];
                    std::snprintf(text, sizeof(text), ": pixel %d,%d is %08x in Qt, ours %08x", x, y, expected.pixel(x, y), actual.pixel(x, y));
                    fail(what + text);
                    return;
                }
            fail(what + ": line " + std::to_string(y) + " differs");
            return;
        }
    }
    if (expected.format() == QImage::Format_Indexed8) {
        const QList<QRgb> table = expected.colorTable();
        if (!std::equal(table.begin(), table.end(), actual.colorTable().begin(), actual.colorTable().end()))
            fail(what + ": colour tables differ");
    }
}


gfx::ColorTable randomTable(int size, bool transparent)
{
    gfx::ColorTable table;
    for (int i = 0; i < size; i++)
        table << gfx::rgba(random(255), random(255), random(255), transparent && random(3) == 0 ? random(255) : 255);
    return table;
}


QList<QRgb> qtTable(const gfx::ColorTable &table)
{
    return QList<QRgb>(table.begin(), table.end());
}


// a random image of the format, as ours and Qt's
std::pair<gfx::Image, QImage> randomImage(gfx::Image::Format format, int width, int height, const gfx::ColorTable &table)
{
    gfx::Image ours(width, height, format);
    QImage qt(width, height, qtFormat(format));
    ours.setColorTable(table);
    if (format == gfx::Image::Indexed8)
        qt.setColorTable(qtTable(table));
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < ours.bytesPerLine(); x++)
            ours.scanLine(y)[x] = std::uint8_t(random(255));
        std::memcpy(qt.scanLine(y), ours.constScanLine(y), ours.bytesPerLine());
    }
    return std::make_pair(ours, qt);
}


const gfx::Image::Format Formats[] = { gfx::Image::Indexed8, gfx::Image::RGB888, gfx::Image::ARGB32, gfx::Image::RGBA8888 };


void testColors()
{
    for (int i = 0; i < 100000; i++) {
        const int r = random(255), g = random(255), b = random(255), a = random(255);
        check(gfx::rgb(r, g, b) == qRgb(r, g, b) && gfx::rgba(r, g, b, a) == qRgba(r, g, b, a), "rgb()");
        const gfx::Rgb c = gfx::rgba(r, g, b, a);
        check(gfx::red(c) == qRed(c) && gfx::green(c) == qGreen(c) && gfx::blue(c) == qBlue(c) && gfx::alpha(c) == qAlpha(c) && gfx::gray(c) == qGray(c), "channels");
    }

    // every colour, by the factor the voxel radar uses, and others on a sample
    for (gfx::Rgb c = 0; c < 0x1000000; c++) {
        const gfx::Rgb rgb = 0xff000000 | c;
        const QColor color(rgb);
        char text[100];
        std::snprintf(text, sizeof(text), "lighter(%08x, 130): Qt %08x, ours %08x", rgb, color.lighter(130).rgba(), gfx::lighter(rgb, 130));
        check(gfx::lighter(rgb, 130) == color.lighter(130).rgba(), text);
        std::snprintf(text, sizeof(text), "darker(%08x, 130): Qt %08x, ours %08x", rgb, color.darker(130).rgba(), gfx::darker(rgb, 130));
        check(gfx::darker(rgb, 130) == color.darker(130).rgba(), text);
    }
    for (int i = 0; i < 1000000; i++) {
        const gfx::Rgb c = gfx::rgba(random(255), random(255), random(255), random(255));
        const int factor = random(400) - 10;
        QColor color(c);
        color.setAlpha(gfx::alpha(c));
        char text[100];
        std::snprintf(text, sizeof(text), "(%08x, %d): Qt %08x, ours %08x", c, factor, color.lighter(factor).rgba(), gfx::lighter(c, factor));
        check(gfx::lighter(c, factor) == color.lighter(factor).rgba(), std::string("lighter") + text);
        std::snprintf(text, sizeof(text), "(%08x, %d): Qt %08x, ours %08x", c, factor, color.darker(factor).rgba(), gfx::darker(c, factor));
        check(gfx::darker(c, factor) == color.darker(factor).rgba(), std::string("darker") + text);
    }
}


void testImages()
{
    // null images
    same("null", QImage(), gfx::Image());
    same("empty", QImage(0, 5, QImage::Format_ARGB32), gfx::Image(0, 5, gfx::Image::ARGB32));
    same("null copy", QImage().copy(0, 0, 3, 3), gfx::Image().copy(0, 0, 3, 3));

    for (int round = 0; round < 2000; round++) {
        const gfx::Image::Format format = Formats[random(3)];
        const int width = 1 + random(40), height = 1 + random(40);
        // colour tables of every size, opaque or not; empty ones are grey levels
        const gfx::ColorTable table = randomTable(random(3) == 0 ? random(256) : 256, random(1) == 0);
        auto [ours, qt] = randomImage(format, width, height, table);
        const std::string what = std::string(name(format)) + " " + std::to_string(width) + "x" + std::to_string(height)
                + " round " + std::to_string(round);
        same(what, qt, ours);

        // reading pixels, also outside
        for (int i = 0; i < 50; i++) {
            const int x = random(width + 3) - 2, y = random(height + 3) - 2;
            if (x < 0 || x >= width || y < 0 || y >= height)
                continue;
            check(ours.pixel(x, y) == qt.pixel(x, y), what + " pixel");
            if (format == gfx::Image::Indexed8)
                check(ours.pixelIndex(x, y) == qt.pixelIndex(x, y), what + " pixelIndex");
        }

        // writing pixels, also outside the image and the colour table (RGBA8888 isn't written to)
        if (format != gfx::Image::RGBA8888) {
            gfx::Image o = ours;
            QImage q = qt.copy();
            for (int i = 0; i < 200; i++) {
                const int x = random(width + 3) - 2, y = random(height + 3) - 2;
                const std::uint32_t value = format == gfx::Image::Indexed8 ? std::uint32_t(random(300) - 20) : std::uint32_t(g_random());
                o.setPixel(x, y, value);
                // Qt warns
                if (x >= 0 && x < width && y >= 0 && y < height && (format != gfx::Image::Indexed8 || value < std::uint32_t(table.size())))
                    q.setPixel(x, y, value);
            }
            same(what + " setPixel", q, o);
        }

        // filling
        if (format == gfx::Image::Indexed8 || format == gfx::Image::ARGB32) {
            const std::uint32_t value = format == gfx::Image::Indexed8 ? std::uint32_t(random(255)) : std::uint32_t(g_random());
            gfx::Image o = ours;
            QImage q = qt.copy();
            o.fill(value);
            q.fill(value);
            same(what + " fill", q, o);
        }
        {
            gfx::Image o = ours;
            QImage q = qt.copy();
            o.fill(0);
            q.fill(0);
            same(what + " fill(0)", q, o);
        }

        // parts, also reaching past the image or outside it (but not 0x0, which QImage takes for
        // all of it)
        for (int i = 0; i < 5; i++) {
            const int x = random(width + 10) - 5, y = random(height + 10) - 5, w = random(width + 5), h = random(height + 5);
            if (w == 0 && h == 0)
                continue;
            same(what + " copy " + std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(w) + "x" + std::to_string(h),
                 qt.copy(x, y, w, h), ours.copy(x, y, w, h));
        }

        // upside down
        same(what + " flipped", qt.flipped(Qt::Vertical), ours.flipped());

        // to the formats the textures, the screenshots and the 3D cockpits take
        same(what + " to RGBA8888", qt.convertToFormat(QImage::Format_RGBA8888), ours.convertToFormat(gfx::Image::RGBA8888));
        if (format != gfx::Image::Indexed8) {
            same(what + " to RGB888", qt.convertToFormat(QImage::Format_RGB888), ours.convertToFormat(gfx::Image::RGB888));
            same(what + " to ARGB32", qt.convertToFormat(QImage::Format_ARGB32), ours.convertToFormat(gfx::Image::ARGB32));
        }

        // doubled, as the fonts are: their pixels are transparent or opaque
        if (format == gfx::Image::ARGB32) {
            gfx::Image o(width, height, gfx::Image::ARGB32);
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    o.setPixel(x, y, random(1) ? 0 : gfx::rgb(random(255), random(255), random(255)));
            QImage q(o.constBits(), width, height, o.bytesPerLine(), QImage::Format_ARGB32);
            same(what + " scaled", q.scaled(width*2, height*2), o.scaled(width*2, height*2));
            same(what + " scaled 1", q.scaled(width, height), o.scaled(width, height));
        }
    }

    // the colour table follows the image
    gfx::Image image(4, 4, gfx::Image::Indexed8);
    image.setColorTable(gfx::ColorTable{ gfx::rgb(1, 2, 3), gfx::rgb(4, 5, 6) });
    image.toRgb565();
    QImage qt(4, 4, QImage::Format_Indexed8);
    qt.fill(0);
    qt.setColorTable(QList<QRgb>() << qRgb(0, 0, 0) << qRgb(0, 4, 0));
    same("toRgb565", qt, image);
}


// the smooth scaling of 3D cockpit textures too large for the GPU, which averages differently
// from Qt's: an even colour stays as it is, and pixels are averaged by how much of them a pixel
// covers, their colours by their alpha
void testSmooth()
{
    for (const gfx::Image::Format format : { gfx::Image::RGB888, gfx::Image::ARGB32, gfx::Image::RGBA8888 }) {
        gfx::Image even(7, 5, format);
        even.fill(gfx::rgb(10, 200, 30));
        for (const auto &size : { std::make_pair(3, 2), std::make_pair(1, 1), std::make_pair(5, 4), std::make_pair(11, 9) }) {
            const gfx::Image scaled = even.scaled(size.first, size.second, true);
            bool same = scaled.width() == size.first && scaled.height() == size.second && scaled.format() == format;
            for (int y = 0; same && y < scaled.height(); y++)
                for (int x = 0; x < scaled.width(); x++)
                    same = same && scaled.pixel(x, y) == gfx::rgb(10, 200, 30);
            check(same, std::string("smooth ") + name(format) + " even colour");
        }
    }
    gfx::Image checker(2, 2, gfx::Image::ARGB32);
    checker.setPixel(0, 0, gfx::rgb(255, 255, 255));
    checker.setPixel(1, 1, gfx::rgb(255, 255, 255));
    checker.setPixel(1, 0, gfx::rgb(0, 0, 0));
    checker.setPixel(0, 1, gfx::rgba(0, 0, 255, 0));
    // white for two quarters, black for one, nothing for the transparent one
    check(checker.scaled(1, 1, true).pixel(0, 0) == gfx::rgba(170, 170, 170, 191), "smooth checker");
    // a third of the middle pixel goes into each side
    gfx::Image line(3, 1, gfx::Image::RGB888);
    line.setPixel(0, 0, gfx::rgb(0, 0, 0));
    line.setPixel(1, 0, gfx::rgb(90, 90, 90));
    line.setPixel(2, 0, gfx::rgb(180, 180, 180));
    const gfx::Image half = line.scaled(2, 1, true);
    check(half.pixel(0, 0) == gfx::rgb(30, 30, 30) && half.pixel(1, 0) == gfx::rgb(150, 150, 150), "smooth thirds");
}


// PNG written by us and read by Qt, and the other way round
void testPng()
{
    QTemporaryDir dir;
    for (int round = 0; round < 50; round++) {
        for (const gfx::Image::Format format : Formats) {
            const int width = 1 + random(60), height = 1 + random(60);
            auto [ours, qt] = randomImage(format, width, height, randomTable(256, true));
            const std::string what = std::string("PNG ") + name(format) + " round " + std::to_string(round);

            const QString file = dir.filePath(QString("%1-%2.png").arg(round).arg(format));
            check(ours.save(file.toStdString()), what + " saved");
            const QImage read(file);
            const bool alpha = format != gfx::Image::RGB888;
            same(what + " read by Qt", read.convertToFormat(alpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888),
                 ours.convertToFormat(alpha ? gfx::Image::RGBA8888 : gfx::Image::RGB888));

            QByteArray data;
            QBuffer buffer(&data);
            buffer.open(QIODevice::WriteOnly);
            qt.save(&buffer, "PNG");
            const gfx::Image decoded = gfx::Image::fromData(reinterpret_cast<const std::uint8_t *>(data.constData()), std::size_t(data.size()));
            const QImage expected = QImage::fromData(data);
            same(what + " written by Qt", expected.convertToFormat(decoded.format() == gfx::Image::RGB888 ? QImage::Format_RGB888 : QImage::Format_RGBA8888), decoded);
        }
    }
    check(gfx::Image::fromData(reinterpret_cast<const std::uint8_t *>("not an image"), 12).isNull(), "no image");
}


// the base colour images of the 3D cockpits: JPEG, which stb_image and Qt's libjpeg decode a
// little differently
void testCockpits(const QString &dataDir)
{
    QDirIterator it(dataDir, QStringList() << "*.glb", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray glb = file.readAll();
        if (glb.size() < 20)
            continue;
        const quint32 jsonLength = qFromLittleEndian<quint32>(glb.constData() + 12);
        const QJsonObject json = QJsonDocument::fromJson(glb.mid(20, int(jsonLength))).object();
        const int binOffset = 20 + int((jsonLength + 3) & ~3u) + 8;
        for (const QJsonValue &value : json["images"].toArray()) {
            const QJsonObject view = json["bufferViews"].toArray()[value.toObject()["bufferView"].toInt()].toObject();
            const QByteArray data = glb.mid(binOffset + view["byteOffset"].toInt(), view["byteLength"].toInt());

            QElapsedTimer timer;
            timer.start();
            const QImage expected = QImage::fromData(data).convertToFormat(QImage::Format_RGB888);
            const qint64 qtTime = timer.restart();
            const gfx::Image actual = gfx::Image::fromData(reinterpret_cast<const std::uint8_t *>(data.constData()), std::size_t(data.size()));
            const qint64 ourTime = timer.elapsed();

            check(!actual.isNull() && actual.width() == expected.width() && actual.height() == expected.height(), "cockpit image size");
            if (actual.isNull() || actual.width() != expected.width() || actual.height() != expected.height())
                continue;
            const gfx::Image rgb = actual.convertToFormat(gfx::Image::RGB888);
            long long sum = 0;
            int maxDifference = 0;
            std::vector<long long> histogram(256);
            for (int y = 0; y < rgb.height(); y++) {
                const std::uint8_t *a = expected.constScanLine(y), *b = rgb.constScanLine(y);
                for (int x = 0; x < rgb.bytesPerLine(); x++) {
                    const int d = std::abs(int(a[x]) - int(b[x]));
                    sum += d;
                    histogram[d]++;
                    maxDifference = std::max(maxDifference, d);
                }
            }
            const double values = double(rgb.bytesPerLine())*rgb.height();
            std::cout << QFileInfo(file.fileName()).fileName().toUtf8().constData() << ": " << rgb.width() << "x" << rgb.height()
                      << ", Qt " << qtTime << " ms, ours " << ourTime << " ms; channels differing by 0/1/2/more: "
                      << histogram[0]/values*100 << "% " << histogram[1]/values*100 << "% " << histogram[2]/values*100 << "% "
                      << (values - histogram[0] - histogram[1] - histogram[2])/values*100 << "%, mean " << sum/values
                      << ", at most " << maxDifference << std::endl;
        }
    }
}


void silence(QtMsgType, const QMessageLogContext &, const QString &)
{
}

} // namespace


int main(int argc, char *argv[])
{
    qInstallMessageHandler(silence);

    testColors();
    testImages();
    testSmooth();
    testPng();
    if (argc > 1)
        testCockpits(QString::fromLocal8Bit(argv[1]));

    for (const auto &kind : g_kinds)
        std::cout << kind.second << " failures of " << kind.first << std::endl;
    std::cout << g_checks << " checks, " << g_failures << " failures" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
