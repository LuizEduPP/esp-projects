/** SQLite rejects `undefined` bindings — normalize to null or string. */
export function sqlText(value, fallback = "") {
  if (value === undefined || value === null) {
    return fallback;
  }
  return String(value);
}

export function sqlNullable(value) {
  return value === undefined ? null : value;
}
