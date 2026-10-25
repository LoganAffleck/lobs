.pragma library
// Audio numbers shared by the mixer and the Inspector's audio settings.

function toDb(lin) { return lin <= 0.0001 ? "-inf" : (20 * Math.log10(lin)).toFixed(1) }
// Fader shaped like OBS's: position 0..1 maps to -60..0 dB.
function posFromLin(lin) { return lin <= 0.001 ? 0 : Math.max(0, Math.min(1, (20 * Math.log10(lin) + 60) / 60)) }
function linFromPos(p) { return p <= 0 ? 0 : Math.pow(10, (p * 60 - 60) / 20) }
function balanceText(b) { const p = Math.round((b - 0.5) * 200); return p === 0 ? "Center" : p < 0 ? "L " + (-p) + "%" : "R " + p + "%" }
