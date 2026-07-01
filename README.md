# ✈️ Flight Passenger Log Conversion Tool

A command-line utility written in C that converts flight passenger records between multiple data formats — CSV, custom binary, and XML — with support for character encoding conversion (UTF-8 ↔ UTF-16LE/BE) and XML Schema (XSD) validation.

## Features

- **CSV → Binary**: Parses delimited passenger log files (comma, tab, or semicolon separated) into a compact fixed-size binary record format, with field-level validation (ticket ID format, seat number, app version).
- **Binary → XML**: Converts binary records into a structured XML document using `libxml2`.
- **XML encoding conversion**: Manually handles BOM detection and byte-level transcoding between UTF-8, UTF-16 Little Endian, and UTF-16 Big Endian — including raw codepoint extraction, without relying on an external transcoding library.
- **Emoji-to-status mapping**: Detects UTF-8 byte sequences for status emojis (🟢 🔴 ⚠️) in raw input and maps them to readable status codes (`BOARDED`, `CANCELLED`, `DELAYED`), and back again on export.
- **XSD Schema Validation**: Validates generated XML documents against a provided `.xsd` schema using `libxml2`'s schema validation context.
- **Configurable CLI**: Supports custom separators, OS line-ending modes, and output encodings via flags.

## Why this project is interesting

Most of the encoding/decoding logic here is implemented manually at the byte level rather than delegated to a library — including UTF-16 BOM detection, endian-aware 16-bit unit reads, and UTF-8 codepoint reconstruction. This was a deliberate choice to better understand how character encodings actually work under the hood, rather than treating them as a black box.

## Build

Requires `libxml2` development headers.

```bash
gcc -o flightTool 2023510145.c $(xml2-config --cflags --libs) -Wall -Wextra
```

## Usage

```bash
./flightTool -h
```

```
Kullanım:
  ./flightTool <input> <output> <type> [options]

type:
  1  CSV → Binary
  2  Binary → XML
  3  Validate XML against XSD
  4  XML → XML (encoding conversion)

Options:
  -separator <1|2|3>   1=comma (default), 2=tab, 3=semicolon
  -opsys     <1|2|3>   Operating system (line ending)
  -encoding  <1|2|3>   1=UTF-16LE, 2=UTF-16BE, 3=UTF-8
```

### Examples

```bash
# CSV to binary
./flightTool flightlog.csv flight.dat 1

# Binary to XML
./flightTool flight.dat flight_utf8.xml 2

# Validate an XML file against a schema
./flightTool flight_utf8.xml schema.xsd 3

# Convert XML encoding
./flightTool flight_utf8.xml flight_utf16.xml 4 -encoding 1
```

## Tech Stack

- **Language**: C
- **Libraries**: `libxml2` (DOM parsing, XML generation, XSD schema validation)
- **Concepts**: Binary file I/O, manual UTF-8/UTF-16 transcoding, CSV parsing, CLI argument handling

## Course Context

Developed as a course assignment (Assignment 1), Computer Engineering Department, Dokuz Eylül University.
CME 2202 DATA ORGANIZATION AND MANAGEMENT

## Authors

Developed as a team assignment by:
- Zeliha E.
- Şengül A.
-  Kamil I.

Computer Engineering, Dokuz Eylül University
