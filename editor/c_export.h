#ifndef C_EXPORT_H
#define C_EXPORT_H

// c_export.h - turns a compiled ladder image into a C header for the PLC firmware
#pragma once
#include <QByteArray>
#include <QString>

// Returns the text of a header that defines `ladder_program[]` and LADDER_PROGRAM_SIZE.
// sourceName is only written into the header comment (may be empty).
// An empty image returns an empty string: a zero-length C array is not valid.
QString imageToCHeader(const QByteArray &image, const QString &sourceName);

#endif // C_EXPORT_H
