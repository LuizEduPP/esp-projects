export const deaccent = (input: string): string =>
  input
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .replace(/[^\x20-\x7e]/g, "")
    .replace(/\s+/g, " ")
    .trim();

export const clamp = (input: string, max: number): string => {
  const clean = deaccent(input);
  if (clean.length <= max) return clean;

  const cut = clean.slice(0, max - 1);
  const space = cut.lastIndexOf(" ");
  const kept = (space > max / 2 ? cut.slice(0, space) : cut).replace(/[\s,;:.-]+$/, "");
  return `${kept}.`;
};
