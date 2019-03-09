#ifndef READXL_COLSPEC_
#define READXL_COLSPEC_

#include <Rcpp.h>
#include <libxls/xls.h>
#include "StringSet.h"

enum CellType {
  CELL_UNKNOWN,
  CELL_BLANK,
  CELL_LOGICAL,
  CELL_DATE,
  CELL_NUMERIC,
  CELL_TEXT
};

enum ColType {
  COL_UNKNOWN, // implies column type needs to be guessed
  COL_BLANK,   // occurs when col_types = NULL and observe only CELL_BLANKs
  COL_LOGICAL,
  COL_DATE,
  COL_NUMERIC,
  COL_TEXT,
  COL_LIST,    // occurs only as user-specified column type
  COL_SKIP     // occurs only as user-specified column type
};

// ColType enum is ordered such that a simple cast is sufficient to convert
// from CellType (user-specified column types come after cell types)
ColType inline as_ColType(CellType celltype) {
  return (ColType) celltype;
}

inline std::string cellTypeDesc(CellType type) {
  switch(type) {
  case CELL_UNKNOWN:  return "unknown";
  case CELL_BLANK:    return "blank";
  case CELL_LOGICAL:  return "logical";
  case CELL_DATE:     return "date";
  case CELL_NUMERIC:  return "numeric";
  case CELL_TEXT:     return "text";
  }
  return "???";
}

inline std::string colTypeDesc(ColType type) {
  switch(type) {
  case COL_UNKNOWN: return "unknown";
  case COL_BLANK:   return "blank";
  case COL_LOGICAL: return "logical";
  case COL_DATE:    return "date";
  case COL_NUMERIC: return "numeric";
  case COL_TEXT:    return "text";
  case COL_LIST:    return "list";
  case COL_SKIP:    return "skip";
  }
  return "???";
}

inline Rcpp::CharacterVector colTypeDescs(std::vector<ColType> types) {
  Rcpp::CharacterVector out(types.size());
  for (size_t i = 0; i < types.size(); ++i) {
    out[i] = colTypeDesc(types[i]);
  }
  return out;
}

inline std::vector<ColType> colTypeStrings(Rcpp::CharacterVector x) {
  std::vector<ColType> types;
  types.reserve(x.size());

  for (int i = 0; i < x.size(); ++i) {
    std::string type(x[i]);
    if (type == "guess") {
      types.push_back(COL_UNKNOWN);
    } else if (type == "blank") {
      types.push_back(COL_BLANK);
    } else if (type == "logical") {
      types.push_back(COL_LOGICAL);
    } else if (type == "date") {
      types.push_back(COL_DATE);
    } else if (type == "numeric") {
      types.push_back(COL_NUMERIC);
    } else if (type == "text") {
      types.push_back(COL_TEXT);
    } else if (type == "list") {
      types.push_back(COL_LIST);
    } else if (type == "skip") {
      types.push_back(COL_SKIP);
    } else {
      Rcpp::stop("Unknown column type '%s' at position %i", type, i + 1);
    }
  }

  return types;
}

bool inline requiresGuess(std::vector<ColType> types) {
  std::vector<ColType>::iterator iter;
  iter = find(types.begin(), types.end(), COL_UNKNOWN);
  return iter != types.end();
}

bool inline isDateTime(int id, const std::set<int> custom) {
  // Page and section numbers below refer to
  // ECMA-376 (version, date, and download URL given in XlsxCell.h)
  //
  // Example from L.2.7.4.4 p4698 for hypothetical cell D2
  // Cell D2 contains the text "Q1" and is defined in the cell table of sheet1
  // as:
  //
  // <c r="D2" s="7" t="s">
  //     <v>0</v>
  // </c>
  //
  // On this cell, the attribute value s="7" indicates that the 7th (zero-based)
  // <xf> definition of <cellXfs> holds the formatting information for the cell.
  // The 7th <xf> of <cellXfs> is defined as:
  //
  // <xf numFmtId="0" fontId="4" fillId="2" borderId="2" xfId="1" applyBorder="1"/>
  //
  // The number formatting information cannot be found in a <numFmt> definition
  // because it is a built-in format; instead, it is implicitly understood to be
  // the 0th built-in number format.
  //
  // This function stores knowledge about these built-in number formats.
  //
  // 18.8.30 numFmt (Number Format) p1786
  // Date times: 14-22, 27-36, 45-47, 50-58, 71-81 (inclusive)
  if ((id >= 14 && id <= 22) ||
      (id >= 27 && id <= 36) ||
      (id >= 45 && id <= 47) ||
      (id >= 50 && id <= 58) ||
      (id >= 71 && id <= 81))
    return true;

  // Built-in format that's not a date
  if (id < 164)
    return false;

  return custom.count(id) > 0;
}

// Adapted from @reviewher https://github.com/tidyverse/readxl/issues/388
#define CASEI(c) case c: case c | 0x20
#define CMPLC(i,n) if(x[i+i] | 0x20 == n)

inline bool isDateFormat(std::string x) {
  // Ideally this would use something like
  // https://github.com/WizardMac/TimeFormatStrings
  // which presumably offers fancier ways to analyze format codes.
  char escaped = 0;
  char bracket = 0;
  for (size_t i = 0; i < x.size(); ++i) switch (x[i]) {
    CASEI('D'):
    CASEI('E'):
    CASEI('H'):
    CASEI('M'):
    CASEI('S'):
    CASEI('Y'):
      if(!escaped && !bracket) return true;
      break;
    case '"':
      escaped = 1 - escaped; break;
    case '\\': ++i; break;
    case '[': if(!escaped) bracket = 1; break;
    case ']': if(!escaped) bracket = 0; break;
    CASEI('G'):
      if(i + 6 < x.size())
      CMPLC(1,'e')
      CMPLC(2,'n')
      CMPLC(3,'e')
      CMPLC(4,'r')
      CMPLC(5,'a')
      CMPLC(6,'l')
        return false;
  }

  return false;
}

#undef CMPLC
#undef CASEI

inline std::vector<ColType> recycleTypes(std::vector<ColType> types,
                                         int ncol) {
  if (types.size() == 1) {
    types.resize(ncol);
    std::fill(types.begin(), types.end(), types[0]);
  }
  return types;
}

inline std::vector<ColType> finalizeTypes(std::vector<ColType> types) {
  // convert blank columns to a default type: logical
  // can only happen when col_type = guess AND
  //   * all cells in column are empty or
  //   * all cells match one of the na strings or
  //   * there is no cell data at all
  for (size_t i = 0; i < types.size(); i++) {
    if (types[i] == COL_BLANK || types[i] == COL_UNKNOWN) {
      types[i] = COL_LOGICAL;
    }
  }
  return types;
}

inline Rcpp::CharacterVector reconcileNames(Rcpp::CharacterVector names,
                                            const std::vector<ColType>& types,
                                            int sheet_i) {
  size_t ncol_names = names.size();
  size_t ncol_types = types.size();

  if (ncol_names == ncol_types) {
    return names;
  }

  size_t ncol_noskip = 0;
  for (size_t i = 0; i < types.size(); i++) {
    if (types[i] != COL_SKIP) {
      ncol_noskip++;
    }
  }
  if (ncol_names != ncol_noskip) {
    Rcpp::stop("Sheet %d has %d columns (%d unskipped), but `col_names` has length %d.",
               sheet_i + 1, ncol_types, ncol_noskip, ncol_names);
  }

  Rcpp::CharacterVector newNames(ncol_types, "");
  size_t j_short = 0;
  for (size_t j_long = 0; j_long < ncol_types; ++j_long) {
    if (types[j_long] == COL_SKIP) {
      continue;
    }
    newNames[j_long] = names[j_short];
    j_short++;
  }
  return newNames;
}

inline Rcpp::RObject makeCol(ColType type, int n) {
  switch(type) {
  case COL_UNKNOWN:
  case COL_BLANK:
  case COL_SKIP:
    return R_NilValue;
  case COL_LOGICAL:
    return Rcpp::LogicalVector(n, NA_LOGICAL);
  case COL_DATE: {
    Rcpp::RObject col = Rcpp::NumericVector(n, NA_REAL);
    col.attr("class") = Rcpp::CharacterVector::create("POSIXct", "POSIXt");
    col.attr("tzone") = "UTC";
    return col;
  }
  case COL_NUMERIC:
    return Rcpp::NumericVector(n, NA_REAL);
  case COL_TEXT:
    return Rcpp::CharacterVector(n, NA_STRING);
  case COL_LIST:
    return Rcpp::List(n, Rcpp::LogicalVector(1, NA_LOGICAL));
  }

  return R_NilValue;
}

inline Rcpp::List removeSkippedColumns(Rcpp::List cols,
                                       Rcpp::CharacterVector names,
                                       std::vector<ColType> types) {
  int p = cols.size();

  int p_out = 0;
  for (int j = 0; j < p; ++j) {
    if (types[j] != COL_SKIP)
      p_out++;
  }

  Rcpp::List out(p_out);
  Rcpp::CharacterVector names_out(p_out);
  int j_out = 0;
  for (int j = 0; j < p; ++j) {
    if (types[j] == COL_SKIP) {
      continue;
    }

    out[j_out] = cols[j];
    names_out[j_out] = names[j];
    j_out++;
  }

  // Turn list into a data frame
  out.attr("names") = names_out;

  return out;
}

#endif
