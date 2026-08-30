export const deaccent = (input: string): string =>
  input
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/[^\x20-\x7e]/g, "")
    .replace(/\s+/g, " ")
    .trim();

export const clamp = (input: string, max: number): string => {
  const clean = deaccent(input);
  return clean.length <= max ? clean : `${clean.slice(0, max - 1).trimEnd()}.`;
};
