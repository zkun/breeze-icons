/*  This file is part of the KDE libraries
 *    Copyright (C) 2016 Kåre Särs <kare.sars@iki.fi>
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public License
 *    along with this library; see the file COPYING.LIB.  If not, write to
 *    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *    Boston, MA 02110-1301, USA.
 */
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>

static QString link(const QString &path, const QString &fileName)
{
    QFile in(path + QLatin1Char('/') + fileName);
    if (!in.open(QIODevice::ReadOnly)) {
        qFatal() << "failed to open" << path << fileName << in.fileName();
        return QString();
    }

    QString firstLine = QString::fromLocal8Bit(in.readLine());
    if (firstLine.isEmpty()) {
        return QString();
    }
    QRegularExpression fNameReg(QStringLiteral("(.*\\.(?:svg|png|gif|ico))$"));
    QRegularExpressionMatch match = fNameReg.match(firstLine);
    if (!match.hasMatch()) {
        return QString();
    }

    QFileInfo linkInfo(path + QLatin1Char('/') + match.captured(1));
    QString aliasLink = link(linkInfo.path(), linkInfo.fileName());
    if (!aliasLink.isEmpty()) {
        // qDebug() <<  fileName << "=" << match.captured(1) << "=" << aliasLink;
        return aliasLink;
    }

    return path + QLatin1Char('/') + match.captured(1);
}

static int parseFile(const QString &indir, const QString &outfile)
{
    QFile out(outfile);
    if (!out.open(QIODevice::WriteOnly)) {
        qFatal() << "Failed to create" << outfile;
    }
    out.write("<!DOCTYPE RCC><RCC version=\"1.0\">\n");
    out.write("<qresource>\n");

    // go to input dir to have proper relative paths
    if (!QDir::setCurrent(indir)) {
        qFatal() << "Failed to switch to input directory" << indir;
    }

    // we look at all interesting files in the indir and create a qrc with resolved symlinks
    QDirIterator it(QStringLiteral("."),
                    {QStringLiteral("*.theme"), QStringLiteral("*.svg"), QStringLiteral("*.png"), QStringLiteral("*.gif"), QStringLiteral("*.ico")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        // ensure nice path without ./ and Co.
        const auto file = QDir::current().relativeFilePath(it.next());
        const QFileInfo fileInfo(file);

        // real symlink resolving for Unices, the rcc compiler ignores such files in -project mode
        if (fileInfo.isSymLink()) {
            const auto linkPath = fileInfo.canonicalFilePath();
            if (linkPath.isEmpty()) {
                qFatal() << "Broken symlink" << file << "in input directory" << indir;
            }
            QString newLine = QStringLiteral("    <file alias=\"%1\">%2</file>\n").arg(file, QDir::current().relativeFilePath(linkPath));
            out.write(newLine.toUtf8());
        }

        // pseudo link files generated by Git on Windows
        else if (const auto aliasLink = link(fileInfo.path(), fileInfo.fileName()); !aliasLink.isEmpty()) {
            QString newLine = QStringLiteral("    <file alias=\"%1\">%2</file>\n").arg(file, QDir::current().relativeFilePath(aliasLink));
            out.write(newLine.toUtf8());
        }

        // normal file
        else {
            QString newLine = QStringLiteral("    <file>%1</file>\n").arg(file);
            out.write(newLine.toUtf8());
        }
    }

    out.write("</qresource>\n");
    out.write("</RCC>\n");
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;

    QCommandLineOption inOption(QStringList() << QLatin1String("i") << QLatin1String("indir"), QStringLiteral("Input directory"), QStringLiteral("indir"));
    QCommandLineOption outOption(QStringList() << QLatin1String("o") << QLatin1String("outfile"), QStringLiteral("Output qrc file"), QStringLiteral("outfile"));
    parser.setApplicationDescription(QLatin1String("Create a resource file from the given input directory handling symlinks and pseudo symlink files."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(inOption);
    parser.addOption(outOption);
    parser.process(app);

    const QString inName = parser.value(inOption);
    const QString outName = parser.value(outOption);
    return parseFile(inName, outName);
}
