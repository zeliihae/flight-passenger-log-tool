/* gcc -o flightTool 2023510145.c $(xml2-config --cflags --libs) -Wall -Wextra
./flightTool flightlog.csv flight.dat 1
./flightTool flight_utf8.xml 2023510145.xsd 3
./flightTool flight.dat flight_utf8.xml 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlschemastypes.h>


typedef struct {
    char    ticket_id[8];        
    char    timestamp[20];      
    float   baggage_weight;      
    int32_t loyalty_points;     
    char    status[12];        
    char    destination[31];     
    char    cabin_class[9];      
    int32_t seat_num;            
    char    app_ver[16];         
    char    passenger_name[256]; 
} PassengerRecord;

void csv_to_binary(const char *input, const char *output, int separator, int opsys);
void binary_to_xml(const char *input, const char *output);
void xml_to_xml(const char *input, const char *output, int encoding);
void validate(const char *XMLFileName, const char *XSDFileName);


static void cleanend(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n'))
        s[--len] = '\0';
}


static void trim(char *s) {
   
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

  
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t'))
        s[--len] = '\0';
}

static char get_separator(int sep) {
    switch (sep) {
        case 2:  return '\t';
        case 3:  return ';';
        default: return ',';  
    }
}

// CSV'deki emoji durumunu yazıya çeviren uTF-8 byte kontrolü
static void parse_status(const char *raw, char *out) {
    unsigned char *u = (unsigned char *)raw;

 
    if      (u[0]==0xF0 && u[1]==0x9F && u[2]==0x9F && u[3]==0xA2)
        strcpy(out, "BOARDED");
    else if (u[0]==0xF0 && u[1]==0x9F && u[2]==0x9F && u[3]==0xA4)
        strcpy(out, "CANCELLED");
    else if (u[0]==0xE2 && u[1]==0x9A && u[2]==0xA0)
        strcpy(out, "DELAYED");
    else {
        //Bilinmeyen emoji
        strncpy(out, raw, 11);
        out[11] = '\0';
    }
}


static int valid_ticket(const char *id) {
    if (strlen(id) != 7) return 0;
    for (int i = 0; i < 3; i++) if (id[i] < 'A' || id[i] > 'Z') return 0;
    for (int i = 3; i < 7; i++) if (id[i] < '0' || id[i] > '9') return 0;
    return 1;
}

static int valid_appver(const char *v) {
    if (v[0] != 'v') return 0;
    int a, b, c;
    return sscanf(v + 1, "%d.%d.%d", &a, &b, &c) == 3;
}

void csv_to_binary(const char *input, const char *output,
                   int separator, int opsys) {
    (void)opsys;  
    FILE *fin = fopen(input, "rb");
    if (!fin) { perror("CSV açılamadı"); return; }

    FILE *fout = fopen(output, "wb");
    if (!fout) { perror("Binary çıktı açılamadı"); fclose(fin); return; }

    char line[1024];
    char sep = get_separator(separator);
    int  row  = 0;

   
    if (!fgets(line, sizeof(line), fin)) { fclose(fin); fclose(fout); return; }

    while (fgets(line, sizeof(line), fin)) {
        cleanend(line);
        printf("İşleme alınan satır: %s\n", line);
        if (line[0] == '\0') continue;

        PassengerRecord rec;
        memset(&rec, 0, sizeof(rec));

        
        char *fields[10];
        char  buf[1024];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        for (int i = 0; i < 10; i++) fields[i] = (char *)"";

        char *p = buf;
        int   nf = 0;
        while (nf < 10) {
            fields[nf++] = p;
            char *q = strchr(p, sep);
            if (!q) break;
            *q = '\0';
            p  = q + 1;
        }

        for (int i = 0; i < nf; i++) trim(fields[i]);

      //ticket id
        if (!valid_ticket(fields[0])) {
            fprintf(stderr, "Satır %d atlandı: geçersiz ticket_id '%s'\n",
                    row + 2, fields[0]);
            continue;
        }
        strncpy(rec.ticket_id, fields[0], 7);
        rec.ticket_id[7] = '\0';

        strncpy(rec.timestamp, fields[1], 19);
        rec.timestamp[19] = '\0';

      
        rec.baggage_weight = (fields[2][0] != '\0') ? (float)atof(fields[2]) : 0.0f;


        rec.loyalty_points = (fields[3][0] != '\0') ? atoi(fields[3]) : -1;

      
        parse_status(fields[4], rec.status);

       
        strncpy(rec.destination, fields[5], 30);
        rec.destination[30] = '\0';

     
        strncpy(rec.cabin_class, fields[6], 8);
        rec.cabin_class[8] = '\0';

       
        if (fields[7][0] == '\0') {
            fprintf(stderr, "Satır %d atlandı: seat_num boş\n", row + 2);
            continue;
        }
        rec.seat_num = atoi(fields[7]);

        
        if (fields[8][0] != '\0' && valid_appver(fields[8])) {
            strncpy(rec.app_ver, fields[8], 15);
            rec.app_ver[15] = '\0';
        }

      
        strncpy(rec.passenger_name, fields[9], 255);
        rec.passenger_name[255] = '\0';

        fwrite(&rec, sizeof(PassengerRecord), 1, fout);
        row++;
    }

    fclose(fin);
    fclose(fout);
    printf("CSV → Binary tamamlandı: %d kayıt yazıldı → %s\n", row, output);
}

// UTF-8 ilk karakterin byte'larını hex'e çevir
static void first_char_hex_utf8(const char *name, char *hex_out) {
    unsigned char *u = (unsigned char *)name;
    int len = 1;
    if      ((u[0] & 0x80) == 0x00) len = 1;
    else if ((u[0] & 0xE0) == 0xC0) len = 2;
    else if ((u[0] & 0xF0) == 0xE0) len = 3;
    else if ((u[0] & 0xF8) == 0xF0) len = 4;

    hex_out[0] = '\0';
    for (int i = 0; i < len; i++) {
        char tmp[3];
        sprintf(tmp, "%02X", u[i]);
        strcat(hex_out, tmp);
    }
}


static const char *status_to_emoji(const char *s) {
    if (strcmp(s, "BOARDED")   == 0) return "\xF0\x9F\x9F\xA2";        
    if (strcmp(s, "CANCELLED") == 0) return "\xF0\x9F\x9F\xA4";        
    if (strcmp(s, "DELAYED")   == 0) return "\xE2\x9A\xA0\xEF\xB8\x8F"; 
    return s;
}

static void root_name_from_file(const char *path, char *out, int max) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    strncpy(out, base, max - 1);
    out[max - 1] = '\0';

    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

void binary_to_xml(const char *input, const char *output) {
    FILE *fin = fopen(input, "rb");
    if (!fin) { perror("Binary dosya açılamadı"); return; }

    char root_name[128];
    root_name_from_file(output, root_name, sizeof(root_name));

    xmlDocPtr  doc  = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST root_name);
    xmlDocSetRootElement(doc, root);

    PassengerRecord rec;
    int  entry_id = 1;
    char tmp[64];

    while (fread(&rec, sizeof(PassengerRecord), 1, fin) == 1) {
       

        xmlNodePtr entry = xmlNewChild(root, NULL, BAD_CAST "entry", NULL);
        sprintf(tmp, "%d", entry_id++);
        xmlNewProp(entry, BAD_CAST "id", BAD_CAST tmp);

      
        xmlNodePtr ticket = xmlNewChild(entry, NULL, BAD_CAST "ticket", NULL);
        xmlNewChild(ticket, NULL, BAD_CAST "ticket_id",   BAD_CAST rec.ticket_id);
        xmlNewChild(ticket, NULL, BAD_CAST "destination", BAD_CAST rec.destination);
        xmlNewChild(ticket, NULL, BAD_CAST "app_ver",     BAD_CAST rec.app_ver);

        
        xmlNodePtr metrics = xmlNewChild(entry, NULL, BAD_CAST "metrics", NULL);
        xmlNewProp(metrics, BAD_CAST "status",
                   BAD_CAST status_to_emoji(rec.status));
        xmlNewProp(metrics, BAD_CAST "cabin_class", BAD_CAST rec.cabin_class);

        sprintf(tmp, "%.1f", rec.baggage_weight);
        xmlNewChild(metrics, NULL, BAD_CAST "baggage_weight", BAD_CAST tmp);

        if (rec.loyalty_points >= 0) {
            sprintf(tmp, "%d", rec.loyalty_points);
            xmlNewChild(metrics, NULL, BAD_CAST "loyalty_points", BAD_CAST tmp);
        } else {
            xmlNewChild(metrics, NULL, BAD_CAST "loyalty_points", BAD_CAST "");
        }

        sprintf(tmp, "%d", rec.seat_num);
        xmlNewChild(metrics, NULL, BAD_CAST "seat_num", BAD_CAST tmp);

        
        xmlNewChild(entry, NULL, BAD_CAST "timestamp", BAD_CAST rec.timestamp);

        char hex[16];
        first_char_hex_utf8(rec.passenger_name, hex);

        xmlNodePtr pname = xmlNewChild(entry, NULL,
                                       BAD_CAST "passenger_name",
                                       BAD_CAST rec.passenger_name);
        xmlNewProp(pname, BAD_CAST "current_encoding", BAD_CAST "UTF-8");
        xmlNewProp(pname, BAD_CAST "first_char_hex",   BAD_CAST hex);
    }

    fclose(fin);

    if (xmlSaveFormatFileEnc(output, doc, "UTF-8", 1) < 0)
        fprintf(stderr, "XML yazılamadı: %s\n", output);
    else
        printf("Binary → XML tamamlandı: %s\n", output);

    xmlFreeDoc(doc);
    xmlCleanupParser();
}

static uint32_t utf8_first_char_to_codepoint(const char *name) {
    unsigned char b0 = (unsigned char)name[0];

    if (b0 < 0x80)
        return b0;
    if ((b0 & 0xE0) == 0xC0)
        return (uint32_t)(b0 & 0x1F) << 6 |
               ((unsigned char)name[1] & 0x3F);
    if ((b0 & 0xF0) == 0xE0)
        return (uint32_t)(b0 & 0x0F) << 12 |
               (uint32_t)((unsigned char)name[1] & 0x3F) << 6 |
               ((unsigned char)name[2] & 0x3F);
    if ((b0 & 0xF8) == 0xF0)
        return (uint32_t)(b0 & 0x07) << 18 |
               (uint32_t)((unsigned char)name[1] & 0x3F) << 12 |
               (uint32_t)((unsigned char)name[2] & 0x3F) << 6 |
               ((unsigned char)name[3] & 0x3F);
    return 0;
}


static void utf16_to_utf8_char(uint16_t cp, char *out, int *len) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        *len = 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        *len = 2;
    } else {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        *len = 3;
    }
}


static void write_utf16(const char *content, const char *output_file, int big_endian) {
    FILE *fp = fopen(output_file, "wb");
    if (!fp) { fprintf(stderr, "Dosya açılamadı: %s\n", output_file); return; }

   
    if (big_endian) { fputc(0xFE, fp); fputc(0xFF, fp); }
    else            { fputc(0xFF, fp); fputc(0xFE, fp); }

    const unsigned char *p = (const unsigned char *)content;
    while (*p) {
        uint32_t cp = 0;
        int bytes = 0;

        if      (*p < 0x80)              { cp = *p;                                                        bytes = 1; }
        else if ((*p & 0xE0) == 0xC0)   { cp = (*p&0x1F)<<6  | (*(p+1)&0x3F);                            bytes = 2; }
        else if ((*p & 0xF0) == 0xE0)   { cp = (*p&0x0F)<<12 | (*(p+1)&0x3F)<<6 | (*(p+2)&0x3F);        bytes = 3; }
        else if ((*p & 0xF8) == 0xF0)   { cp = (*p&0x07)<<18 | (*(p+1)&0x3F)<<12| (*(p+2)&0x3F)<<6|(*(p+3)&0x3F); bytes = 4; }
        p += bytes;

        if (cp >= 0x10000) {
            cp -= 0x10000;
            uint16_t high = (uint16_t)(0xD800 | (cp >> 10));
            uint16_t low  = (uint16_t)(0xDC00 | (cp & 0x3FF));
            if (big_endian) {
                fputc((high >> 8) & 0xFF, fp); fputc(high & 0xFF, fp);
                fputc((low  >> 8) & 0xFF, fp); fputc(low  & 0xFF, fp);
            } else {
                fputc(high & 0xFF, fp); fputc((high >> 8) & 0xFF, fp);
                fputc(low  & 0xFF, fp); fputc((low  >> 8) & 0xFF, fp);
            }
        } else {
            uint16_t unit = (uint16_t)cp;
            if (big_endian) { fputc((unit >> 8) & 0xFF, fp); fputc(unit & 0xFF, fp); }
            else             { fputc(unit & 0xFF, fp); fputc((unit >> 8) & 0xFF, fp); }
        }
    }
    fclose(fp);
}

// content içinde ilk eşleşmeyi güvenli şekilde değiştir
static char *safe_replace_first(char **buf_ptr, size_t *buf_size,
                                 const char *old_str, const char *new_str) {
    char *pos = strstr(*buf_ptr, old_str);
    if (!pos) return NULL;

    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    size_t offset  = (size_t)(pos - *buf_ptr);
    size_t tail    = strlen(pos + old_len) + 1; 

    if (new_len > old_len) {
        size_t needed = strlen(*buf_ptr) + (new_len - old_len) + 1;
        if (needed > *buf_size) {
            *buf_size = needed * 2;
            char *tmp = realloc(*buf_ptr, *buf_size);
            if (!tmp) { fprintf(stderr, "realloc hatası\n"); return NULL; }
            *buf_ptr = tmp;
            pos = *buf_ptr + offset;
        }
    }

    memmove(pos + new_len, pos + old_len, tail);
    memcpy(pos, new_str, new_len);
    return pos + new_len;
}

static void update_all_passenger_attrs(char **content, size_t *buf_size,
                                        const char *new_encoding,
                                        const char *new_hex) {
    char *search_from = *content;

    while ((search_from = strstr(search_from, "<passenger_name")) != NULL) {
        char *tag_end = strchr(search_from, '>');
        if (!tag_end) break;

        //current_encoding güncelle
        char *enc_pos = strstr(search_from, "current_encoding=\"");
        if (enc_pos && enc_pos < tag_end) {
            enc_pos += strlen("current_encoding=\"");
            char *end_quote = strchr(enc_pos, '"');
            if (end_quote && end_quote < tag_end) {
                //Eski değeri bul 
                size_t old_val_len = (size_t)(end_quote - enc_pos);
                char old_attr[64], new_attr[64];
                snprintf(old_attr, sizeof(old_attr), "current_encoding=\"%.*s\"",
                         (int)old_val_len, enc_pos);
                snprintf(new_attr, sizeof(new_attr), "current_encoding=\"%s\"",
                         new_encoding);
                size_t offset = (size_t)(search_from - *content);
                safe_replace_first(content, buf_size, old_attr, new_attr);
                search_from = *content + offset; // pointer yenile 
                tag_end = strchr(search_from, '>');
                if (!tag_end) break;
            }
        }

        char *hex_pos = strstr(search_from, "first_char_hex=\"");
        if (hex_pos && hex_pos < tag_end) {
            hex_pos += strlen("first_char_hex=\"");
            char *end_quote = strchr(hex_pos, '"');
            if (end_quote && end_quote < tag_end) {
                size_t old_val_len = (size_t)(end_quote - hex_pos);
                char old_attr[64], new_attr[64];
                snprintf(old_attr, sizeof(old_attr), "first_char_hex=\"%.*s\"",
                         (int)old_val_len, hex_pos);
                snprintf(new_attr, sizeof(new_attr), "first_char_hex=\"%s\"",
                         new_hex);
                size_t offset = (size_t)(search_from - *content);
                safe_replace_first(content, buf_size, old_attr, new_attr);
                search_from = *content + offset;
                tag_end = strchr(search_from, '>');
                if (!tag_end) break;
            }
        }

        search_from = tag_end + 1;
    }
}

static void update_xml_declaration(char **content, size_t *buf_size,
                                    const char *new_encoding) {
    char *enc_pos = strstr(*content, "encoding=\"");
    if (!enc_pos) return;

    enc_pos += strlen("encoding=\"");
    char *end_quote = strchr(enc_pos, '"');
    if (!end_quote) return;

    size_t old_val_len = (size_t)(end_quote - enc_pos);
    char old_attr[64], new_attr[64];
    snprintf(old_attr, sizeof(old_attr), "encoding=\"%.*s\"",
             (int)old_val_len, enc_pos);
    snprintf(new_attr, sizeof(new_attr), "encoding=\"%s\"", new_encoding);
    safe_replace_first(content, buf_size, old_attr, new_attr);
}

// UTF-16 dosyayı okuyup UTF-8 string'e çevirir
static char *read_utf16_file(const char *filename, int *big_endian) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    unsigned char bom[2];
    fread(bom, 1, 2, fp);
    if      (bom[0] == 0xFF && bom[1] == 0xFE) *big_endian = 0;
    else if (bom[0] == 0xFE && bom[1] == 0xFF) *big_endian = 1;
    else { fclose(fp); return NULL; }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp) - 2;
    fseek(fp, 2, SEEK_SET);

    int unit_count = (int)(file_size / 2);
    uint16_t *units = malloc((size_t)unit_count * sizeof(uint16_t));
    if (!units) { fclose(fp); return NULL; }

    for (int i = 0; i < unit_count; i++) {
        unsigned char lo, hi;
        fread(&lo, 1, 1, fp);
        fread(&hi, 1, 1, fp);
        units[i] = *big_endian ? ((uint16_t)lo << 8) | hi
                               : ((uint16_t)hi << 8) | lo;
    }
    fclose(fp);

    // UTF-8'e çevir
    char *utf8 = malloc((size_t)unit_count * 4 + 1);
    if (!utf8) { free(units); return NULL; }

    int pos = 0;
    for (int i = 0; i < unit_count; i++) {
        int len = 0;
        char buf[4];
        utf16_to_utf8_char(units[i], buf, &len);
        memcpy(utf8 + pos, buf, (size_t)len);
        pos += len;
    }
    utf8[pos] = '\0';
    free(units);
    return utf8;
}

void xml_to_xml(const char *input_file, const char *output_file, int encoding) {
    char  *content = NULL;
    size_t buf_size = 0;
    int    was_big_endian = 0;

    //BOM kontrolü 
    FILE *fp = fopen(input_file, "rb");
    if (!fp) { fprintf(stderr, "Dosya açılamadı: %s\n", input_file); return; }
    unsigned char bom[2];
    fread(bom, 1, 2, fp);
    fclose(fp);

    int is_utf16 = (bom[0] == 0xFF && bom[1] == 0xFE) ||
                   (bom[0] == 0xFE && bom[1] == 0xFF);

    if (is_utf16) {
        content = read_utf16_file(input_file, &was_big_endian);
        if (!content) { fprintf(stderr, "UTF-16 okunamadı\n"); return; }
        buf_size = strlen(content) + 1;
    } else {
        fp = fopen(input_file, "rb");
        if (!fp) { fprintf(stderr, "Dosya açılamadı\n"); return; }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        rewind(fp);
        buf_size = (size_t)sz * 2 + 256;
        content = malloc(buf_size);
        if (!content) { fclose(fp); return; }
        fread(content, 1, (size_t)sz, fp);
        content[sz] = '\0';
        fclose(fp);
    }

    // passenger name içindeki karakteri al 
    char first_char_utf8[8] = {0};
    char *pname_start = strstr(content, "<passenger_name");
    if (pname_start) {
        char *text_start = strchr(pname_start, '>');
        if (text_start) {
            text_start++;
            unsigned char b = (unsigned char)text_start[0];
            int char_len = 1;
            if      ((b & 0x80) == 0x00) char_len = 1;
            else if ((b & 0xE0) == 0xC0) char_len = 2;
            else if ((b & 0xF0) == 0xE0) char_len = 3;
            else if ((b & 0xF8) == 0xF0) char_len = 4;
            memcpy(first_char_utf8, text_start, (size_t)char_len);
        }
    }

    // Yeni hex ve encoding hesapla
    char new_hex[16]      = {0};
    char new_enc_name[16] = {0};

    uint32_t cp = utf8_first_char_to_codepoint(first_char_utf8);

if (encoding == 1) { //UTF-16 Little Endian 
    uint16_t unit = (uint16_t)cp;
    sprintf(new_hex, "%02X%02X", unit & 0xFF, (unit >> 8) & 0xFF); 
    strcpy(new_enc_name, "UTF-16LE");
    update_xml_declaration(&content, &buf_size, "UTF-16LE");
} else if (encoding == 2) { //UTF-16 Big Endian 
    uint16_t unit = (uint16_t)cp;
    sprintf(new_hex, "%02X%02X", (unit >> 8) & 0xFF, unit & 0xFF); 
    strcpy(new_enc_name, "UTF-16BE");
    update_xml_declaration(&content, &buf_size, "UTF-16BE");
} else if (encoding == 3) { //UTF-8
    unsigned char *u8p = (unsigned char *)first_char_utf8;
    char tmp[16] = "";
    for(int i = 0; u8p[i] && i < 4; i++) {
        char byte_str[3];
        sprintf(byte_str, "%02X", u8p[i]);
        strcat(tmp, byte_str);
    }
    strcpy(new_hex, tmp);
    strcpy(new_enc_name, "UTF-8");
    update_xml_declaration(&content, &buf_size, "UTF-8");
}

    update_all_passenger_attrs(&content, &buf_size, new_enc_name, new_hex);

    //Dosyaya yaz 
    if (encoding == 1) {
        write_utf16(content, output_file, 0);
    } else if (encoding == 2) {
        write_utf16(content, output_file, 1);
    } else {
        FILE *out = fopen(output_file, "wb");
        if (!out) { fprintf(stderr, "Çıktı açılamadı: %s\n", output_file); }
        else {
            fputs(content, out);
            fclose(out);
        }
    }

    free(content);
    printf("Dönüşüm tamamlandı: %s\n", output_file);
}

void validate(const char *XMLFileName, const char *XSDFileName) {
    xmlDocPtr             doc    = NULL;
    xmlSchemaPtr          schema = NULL;
    xmlSchemaParserCtxtPtr pctxt;

    xmlLineNumbersDefault(1);

    pctxt  = xmlSchemaNewParserCtxt(XSDFileName);
    schema = xmlSchemaParse(pctxt);
    xmlSchemaFreeParserCtxt(pctxt);

    doc = xmlReadFile(XMLFileName, NULL, 0);
    if (!doc) {
        fprintf(stderr, "Could not parse %s\n", XMLFileName);
    } else {
        xmlSchemaValidCtxtPtr vctxt;
        int ret;

        vctxt = xmlSchemaNewValidCtxt(schema);
        ret   = xmlSchemaValidateDoc(vctxt, doc);

        if      (ret == 0) printf("%s validates\n", XMLFileName);
        else if (ret  > 0) printf("%s fails to validate\n", XMLFileName);
        else               printf("%s validation generated an internal error\n", XMLFileName);

        xmlSchemaFreeValidCtxt(vctxt);
        xmlFreeDoc(doc);
    }

    if (schema) xmlSchemaFree(schema);
    xmlSchemaCleanupTypes();
    xmlCleanupParser();
    xmlMemoryDump();

    return;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Kullanım: ./flightTool -h\n");
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0) {
        printf(
            "Kullanım:\n"
            "  ./flightTool <input> <output> <type> [seçenekler]\n\n"
            "type:\n"
            "  1  CSV → Binary\n"
            "  2  Binary → XML\n"
            "  3 validate cml xsd  \n"
            "  4  XML → XML (encoding dönüşümü)\n\n"
            "Seçenekler:\n"
            "  -separator <1|2|3>   1=virgül (varsayılan), 2=tab, 3=noktalı virgül\n"
            "  -opsys     <1|2|3>   İşletim sistemi (satır sonu)\n"
            "  -encoding  <1|2|3>   1=UTF-16LE, 2=UTF-16BE, 3=UTF-8\n"
        );
        return 0;
    }

    if (argc < 4) {
        fprintf(stderr, "Hata: Eksik argüman. Kullanım./flightTool -h menü için için: ./flightTool -h\n");
        return 1;
    }

    char *input_file    = argv[1];
    char *output_file   = argv[2];
    int   conversion_type = atoi(argv[3]);

    int separator = 1, opsys = 1, encoding = 3;

    for (int i = 4; i < argc - 1; i++) {
        if      (strcmp(argv[i], "-separator") == 0) separator = atoi(argv[++i]);
        else if (strcmp(argv[i], "-opsys")     == 0) opsys     = atoi(argv[++i]);
        else if (strcmp(argv[i], "-encoding")  == 0) encoding  = atoi(argv[++i]);
    }

    switch (conversion_type) {
        case 1: csv_to_binary(input_file, output_file, separator, opsys); break;
        case 2: binary_to_xml(input_file, output_file);                   break;
        case 3: validate(input_file, output_file);                        break;
        case 4: xml_to_xml   (input_file, output_file, encoding);         break;
        default:
            fprintf(stderr, "Geçersiz conversion type: %d\n", conversion_type);
            return 1;
    }

    return 0;
}