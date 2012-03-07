/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2011 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include <stdlib.h>
#include <language_table.h>

char const * const bg_language_codes[] =
  {
 "aar",
 "abk",
 "ace",
 "ach",
 "ada",
 "afa",
 "afh",
 "afr",
 "aka",
 "akk",
 "ale",
 "alg",
 "amh",
 "ang",
 "apa",
 "ara",
 "arc",
 "arn",
 "arp",
 "art",
 "arw",
 "asm",
 "ath",
 "aus",
 "ava",
 "ave",
 "awa",
 "aym",
 "aze",
 "bad",
 "bai",
 "bak",
 "bal",
 "bam",
 "ban",
 "bas",
 "bat",
 "bej",
 "bel",
 "bem",
 "ben",
 "ber",
 "bho",
 "bih",
 "bik",
 "bin",
 "bis",
 "bla",
 "bnt",
 "tib",
 "bos",
 "bra",
 "bre",
 "btk",
 "bua",
 "bug",
 "bul",
 "cad",
 "cai",
 "car",
 "cat",
 "cau",
 "ceb",
 "cel",
 "cze",
 "cha",
 "chb",
 "che",
 "chg",
 "chk",
 "chm",
 "chn",
 "cho",
 "chp",
 "chr",
 "chu",
 "chv",
 "chy",
 "cmc",
 "cop",
 "cor",
 "cos",
 "cpe",
 "cpf",
 "cpp",
 "cre",
 "crp",
 "cus",
 "wel",
 "dak",
 "dan",
 "day",
 "del",
 "den",
 "ger",
 "dgr",
 "din",
 "div",
 "doi",
 "dra",
 "dua",
 "dum",
 "dyu",
 "dzo",
 "efi",
 "egy",
 "eka",
 "gre",
 "elx",
 "eng",
 "enm",
 "epo",
 "est",
 "baq",
 "ewe",
 "ewo",
 "fan",
 "fao",
 "per",
 "fat",
 "fij",
 "fin",
 "fiu",
 "fon",
 "fre",
 "frm",
 "fro",
 "fry",
 "ful",
 "fur",
 "gaa",
 "gay",
 "gba",
 "gem",
 "gez",
 "gil",
 "gla",
 "gle",
 "glg",
 "glv",
 "gmh",
 "goh",
 "gon",
 "gor",
 "got",
 "grb",
 "grc",
 "grn",
 "guj",
 "gwi",
 "hai",
 "hau",
 "haw",
 "heb",
 "her",
 "hil",
 "him",
 "hin",
 "hit",
 "hmn",
 "hmo",
 "scr",
 "hun",
 "hup",
 "arm",
 "iba",
 "ibo",
 "ijo",
 "iku",
 "ile",
 "ilo",
 "ina",
 "inc",
 "ind",
 "ine",
 "ipk",
 "ira",
 "iro",
 "ice",
 "ita",
 "jav",
 "jpn",
 "jpr",
 "kaa",
 "kab",
 "kac",
 "kal",
 "kam",
 "kan",
 "kar",
 "kas",
 "geo",
 "kau",
 "kaw",
 "kaz",
 "kha",
 "khi",
 "khm",
 "kho",
 "kik",
 "kin",
 "kir",
 "kmb",
 "kok",
 "kom",
 "kon",
 "kor",
 "kos",
 "kpe",
 "kro",
 "kru",
 "kum",
 "kur",
 "kut",
 "lad",
 "lah",
 "lam",
 "lao",
 "lat",
 "lav",
 "lez",
 "lin",
 "lit",
 "lol",
 "loz",
 "ltz",
 "lua",
 "lub",
 "lug",
 "lui",
 "lun",
 "luo",
 "lus",
 "mad",
 "mag",
 "mah",
 "mai",
 "mak",
 "mal",
 "man",
 "map",
 "mar",
 "mas",
 "mdr",
 "men",
 "mga",
 "mic",
 "min",
 "mis",
 "mac",
 "mkh",
 "mlg",
 "mlt",
 "mnc",
 "mni",
 "mno",
 "moh",
 "mol",
 "mon",
 "mos",
 "mao",
 "may",
 "mul",
 "mun",
 "mus",
 "mwr",
 "bur",
 "myn",
 "nah",
 "nai",
 "nau",
 "nav",
 "nbl",
 "nde",
 "ndo",
 "nds",
 "nep",
 "new",
 "nia",
 "nic",
 "niu",
 "dut",
 "nno",
 "nob",
 "non",
 "nor",
 "nso",
 "nub",
 "nya",
 "nym",
 "nyn",
 "nyo",
 "nzi",
 "oci",
 "oji",
 "ori",
 "orm",
 "osa",
 "oss",
 "ota",
 "oto",
 "paa",
 "pag",
 "pal",
 "pam",
 "pan",
 "pap",
 "pau",
 "peo",
 "phi",
 "pli",
 "pol",
 "pon",
 "por",
 "pra",
 "pro",
 "pus",
 "que",
 "raj",
 "rap",
 "rar",
 "roa",
 "rom",
 "rum",
 "run",
 "rus",
 "sad",
 "sag",
 "sah",
 "sai",
 "sal",
 "sam",
 "san",
 "sas",
 "sat",
 "sco",
 "sel",
 "sem",
 "sga",
 "sgn",
 "shn",
 "sid",
 "sin",
 "sio",
 "sit",
 "sla",
 "slo",
 "slv",
 "sme",
 "smi",
 "smo",
 "sna",
 "snd",
 "snk",
 "sog",
 "som",
 "son",
 "sot",
 "spa",
 "alb",
 "srd",
 "scc",
 "srr",
 "ssa",
 "ssw",
 "suk",
 "sun",
 "sus",
 "sux",
 "swa",
 "swe",
 "syr",
 "tah",
 "tai",
 "tam",
 "tat",
 "tel",
 "tem",
 "ter",
 "tet",
 "tgk",
 "tgl",
 "tha",
 "tig",
 "tir",
 "tiv",
 "tkl",
 "tli",
 "tmh",
 "tog",
 "ton",
 "tpi",
 "tsi",
 "tsn",
 "tso",
 "tuk",
 "tum",
 "tur",
 "tut",
 "tvl",
 "twi",
 "tyv",
 "uga",
 "uig",
 "ukr",
 "umb",
 "und",
 "urd",
 "uzb",
 "vai",
 "ven",
 "vie",
 "vol",
 "vot",
 "wak",
 "wal",
 "war",
 "was",
 "wen",
 "wol",
 "xho",
 "yao",
 "yap",
 "yid",
 "yor",
 "ypk",
 "zap",
 "zen",
 "zha",
 "chi",
 "znd",
 "zul",
 "zun",
 NULL,
  };
char const * const bg_language_labels[] =
  {
 "Afar",
 "Abkhazian",
 "Achinese",
 "Acoli",
 "Adangme",
 "Afro-Asiatic (Other)",
 "Afrihili",
 "Afrikaans",
 "Akan",
 "Akkadian",
 "Aleut",
 "Algonquian languages",
 "Amharic",
 "English, Old (ca. 450-1100)",
 "Apache languages",
 "Arabic",
 "Aramaic",
 "Araucanian",
 "Arapaho",
 "Artificial (Other)",
 "Arawak",
 "Assamese",
 "Athapascan languages",
 "Australian languages",
 "Avaric",
 "Avestan",
 "Awadhi",
 "Aymara",
 "Azerbaijani",
 "Banda",
 "Bamileke languages",
 "Bashkir",
 "Baluchi",
 "Bambara",
 "Balinese",
 "Basa",
 "Baltic (Other)",
 "Beja",
 "Belarusian",
 "Bemba",
 "Bengali",
 "Berber (Other)",
 "Bhojpuri",
 "Bihari",
 "Bikol",
 "Bini",
 "Bislama",
 "Siksika",
 "Bantu (Other)",
 "Tibetan",
 "Bosnian",
 "Braj",
 "Breton",
 "Batak (Indonesia)",
 "Buriat",
 "Buginese",
 "Bulgarian",
 "Caddo",
 "Central American Indian (Other)",
 "Carib",
 "Catalan",
 "Caucasian (Other)",
 "Cebuano",
 "Celtic (Other)",
 "Czech",
 "Chamorro",
 "Chibcha",
 "Chechen",
 "Chagatai",
 "Chuukese",
 "Mari",
 "Chinook jargon",
 "Choctaw",
 "Chipewyan",
 "Cherokee",
 "Church Slavic",
 "Chuvash",
 "Cheyenne",
 "Chamic languages",
 "Coptic",
 "Cornish",
 "Corsican",
 "Creoles and pidgins, English based (Other)",
 "Creoles and pidgins, French-based (Other)",
 "Creoles and pidgins, Portuguese-based (Other)",
 "Cree",
 "Creoles and pidgins (Other)",
 "Cushitic (Other)",
 "Welsh",
 "Dakota",
 "Danish",
 "Dayak",
 "Delaware",
 "Slave (Athapascan)",
 "German",
 "Dogrib",
 "Dinka",
 "Divehi",
 "Dogri",
 "Dravidian (Other)",
 "Duala",
 "Dutch, Middle (ca. 1050-1350)",
 "Dyula",
 "Dzongkha",
 "Efik",
 "Egyptian (Ancient)",
 "Ekajuk",
 "Greek, Modern (1453-)",
 "Elamite",
 "English",
 "English, Middle (1100-1500)",
 "Esperanto",
 "Estonian",
 "Basque",
 "Ewe",
 "Ewondo",
 "Fang",
 "Faroese",
 "Persian",
 "Fanti",
 "Fijian",
 "Finnish",
 "Finno-Ugrian (Other)",
 "Fon",
 "French",
 "French, Middle (ca. 1400-1600)",
 "French, Old (842-ca. 1400)",
 "Frisian",
 "Fulah",
 "Friulian",
 "Ga",
 "Gayo",
 "Gbaya",
 "Germanic (Other)",
 "Geez",
 "Gilbertese",
 "Gaelic (Scots)",
 "Irish",
 "Gallegan",
 "Manx",
 "German, Middle High (ca. 1050-1500)",
 "German, Old High (ca. 750-1050)",
 "Gondi",
 "Gorontalo",
 "Gothic",
 "Grebo",
 "Greek, Ancient (to 1453)",
 "Guarani",
 "Gujarati",
 "Gwich´in",
 "Haida",
 "Hausa",
 "Hawaiian",
 "Hebrew",
 "Herero",
 "Hiligaynon",
 "Himachali",
 "Hindi",
 "Hittite",
 "Hmong",
 "Hiri Motu",
 "Croatian",
 "Hungarian",
 "Hupa",
 "Armenian",
 "Iban",
 "Igbo",
 "Ijo",
 "Inuktitut",
 "Interlingue",
 "Iloko",
 "Interlingua (International Auxiliary Language Association)",
 "Indic (Other)",
 "Indonesian",
 "Indo-European (Other)",
 "Inupiaq",
 "Iranian (Other)",
 "Iroquoian languages",
 "Icelandic",
 "Italian",
 "Javanese",
 "Japanese",
 "Judeo-Persian",
 "Kara-Kalpak",
 "Kabyle",
 "Kachin",
 "Kalaallisut",
 "Kamba",
 "Kannada",
 "Karen",
 "Kashmiri",
 "Georgian",
 "Kanuri",
 "Kawi",
 "Kazakh",
 "Khasi",
 "Khoisan (Other)",
 "Khmer",
 "Khotanese",
 "Kikuyu",
 "Kinyarwanda",
 "Kirghiz",
 "Kimbundu",
 "Konkani",
 "Komi",
 "Kongo",
 "Korean",
 "Kosraean",
 "Kpelle",
 "Kru",
 "Kurukh",
 "Kumyk",
 "Kurdish",
 "Kutenai",
 "Ladino",
 "Lahnda",
 "Lamba",
 "Lao",
 "Latin",
 "Latvian",
 "Lezghian",
 "Lingala",
 "Lithuanian",
 "Mongo",
 "Lozi",
 "Letzeburgesch",
 "Luba-Lulua",
 "Luba-Katanga",
 "Ganda",
 "Luiseno",
 "Lunda",
 "Luo (Kenya and Tanzania)",
 "lushai",
 "Madurese",
 "Magahi",
 "Marshall",
 "Maithili",
 "Makasar",
 "Malayalam",
 "Mandingo",
 "Austronesian (Other)",
 "Marathi",
 "Masai",
 "Mandar",
 "Mende",
 "Irish, Middle (900-1200)",
 "Micmac",
 "Minangkabau",
 "Miscellaneous languages",
 "Macedonian",
 "Mon-Khmer (Other)",
 "Malagasy",
 "Maltese",
 "Manchu",
 "Manipuri",
 "Manobo languages",
 "Mohawk",
 "Moldavian",
 "Mongolian",
 "Mossi",
 "Maori",
 "Malay",
 "Multiple languages",
 "Munda languages",
 "Creek",
 "Marwari",
 "Burmese",
 "Mayan languages",
 "Nahuatl",
 "North American Indian",
 "Nauru",
 "Navajo",
 "Ndebele, South",
 "Ndebele, North",
 "Ndonga",
 "Low German; Low Saxon; German, Low; Saxon, Low",
 "Nepali",
 "Newari",
 "Nias",
 "Niger-Kordofanian (Other)",
 "Niuean",
 "Dutch",
 "Norwegian Nynorsk",
 "Norwegian Bokmål",
 "Norse, Old",
 "Norwegian",
 "Sotho, Northern",
 "Nubian languages",
 "Chichewa; Nyanja",
 "Nyamwezi",
 "Nyankole",
 "Nyoro",
 "Nzima",
 "Occitan (post 1500); Provençal",
 "Ojibwa",
 "Oriya",
 "Oromo",
 "Osage",
 "Ossetian; Ossetic",
 "Turkish, Ottoman (1500-1928)",
 "Otomian languages",
 "Papuan (Other)",
 "Pangasinan",
 "Pahlavi",
 "Pampanga",
 "Panjabi",
 "Papiamento",
 "Palauan",
 "Persian, Old (ca. 600-400 b.c.)",
 "Philippine (Other)",
 "Pali",
 "Polish",
 "Pohnpeian",
 "Portuguese",
 "Prakrit languages",
 "Provençal, Old (to 1500)",
 "Pushto",
 "Quechua",
 "Rajasthani",
 "Rapanui",
 "Rarotongan",
 "Romance (Other)",
 "Romany",
 "Romanian",
 "Rundi",
 "Russian",
 "Sandawe",
 "Sango",
 "Yakut",
 "South American Indian (Other)",
 "Salishan languages",
 "Samaritan Aramaic",
 "Sanskrit",
 "Sasak",
 "Santali",
 "Scots",
 "Selkup",
 "Semitic (Other)",
 "Irish, Old (to 900)",
 "Sign Languages",
 "Shan",
 "Sidamo",
 "Sinhalese",
 "Siouan languages",
 "Sino-Tibetan (Other)",
 "Slavic (Other)",
 "Slovak",
 "Slovenian",
 "Northern Sami",
 "Sami languages (Other)",
 "Samoan",
 "Shona",
 "Sindhi",
 "Soninke",
 "Sogdian",
 "Somali",
 "Songhai",
 "Sotho, Southern",
 "Spanish",
 "Albanian",
 "Sardinian",
 "Serbian",
 "Serer",
 "Nilo-Saharan (Other)",
 "Swati",
 "Sukuma",
 "Sundanese",
 "Susu",
 "Sumerian",
 "Swahili",
 "Swedish",
 "Syriac",
 "Tahitian",
 "Tai (Other)",
 "Tamil",
 "Tatar",
 "Telugu",
 "Timne",
 "Tereno",
 "Tetum",
 "Tajik",
 "Tagalog",
 "Thai",
 "Tigre",
 "Tigrinya",
 "Tiv",
 "Tokelau",
 "Tlingit",
 "Tamashek",
 "Tonga (Nyasa)",
 "Tonga (Tonga Islands)",
 "Tok Pisin",
 "Tsimshian",
 "Tswana",
 "Tsonga",
 "Turkmen",
 "Tumbuka",
 "Turkish",
 "Altaic (Other)",
 "Tuvalu",
 "Twi",
 "Tuvinian",
 "Ugaritic",
 "Uighur",
 "Ukrainian",
 "Umbundu",
 "Undetermined",
 "Urdu",
 "Uzbek",
 "Vai",
 "Venda",
 "Vietnamese",
 "Volapük",
 "Votic",
 "Wakashan languages",
 "Walamo",
 "Waray",
 "Washo",
 "Sorbian languages",
 "Wolof",
 "Xhosa",
 "Yao",
 "Yapese",
 "Yiddish",
 "Yoruba",
 "Yupik languages",
 "Zapotec",
 "Zenaga",
 "Zhuang",
 "Chinese",
 "Zande",
 "Zulu",
 "Zuni",
 NULL,
  };

