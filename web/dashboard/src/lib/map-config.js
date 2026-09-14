/** Visual-only configuration. No business metrics are derived from these values. */
export const MAP_CONFIG = Object.freeze({
  network: Object.freeze({maxKm:150, maxDegree:3, candidateCap:72}),
  renderer: Object.freeze({maxFPS:30, maxDPR:1.75, pixelBudget:5500000}),
  lighting: Object.freeze({exposure:.66, denseThreshold:500, densityCell:10, densityExponent:.52}),
  // A cinematic national overview extends beneath floating cards. Province detail fits the clear
  // center. Each number below is a viewport fraction, not a geographical coordinate offset.
  cinematicFrame: Object.freeze({x:.12, y:.235, width:.76, height:.61}),
  extrusion: Object.freeze({relativeDepth:.020, minDepth:10, maxDepth:22})
});
