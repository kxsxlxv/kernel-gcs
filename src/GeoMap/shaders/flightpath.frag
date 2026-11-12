VARYING float vHighlight;
VARYING float vDistancePx;
VARYING float vFade;
VARYING float vSide;

void MAIN()
{
    // trailStyle: 0 solid, 1 dashed, 2 dotted, 3 fading.
    if (trailStyle > 0.5 && trailStyle < 1.5 && mod(vDistancePx, 16.0) > 10.0) {
        discard;
    }
    if (trailStyle > 1.5 && trailStyle < 2.5) {
        float repeatPx = max(lineWidth * 2.5, 7.0);
        float distanceToCenterPx = min(mod(vDistancePx, repeatPx), repeatPx - mod(vDistancePx, repeatPx));
        vec2 circleCoord = vec2(distanceToCenterPx / max(lineWidth * 0.5, 0.5), vSide);
        if (dot(circleCoord, circleCoord) > 1.0) {
            discard;
        }
    }
    vec3 linearColor = clamp(mix(pathColor.rgb, highlightColor.rgb, vHighlight), 0.0, 1.0);
    vec3 srgb = mix(linearColor * 12.92, 1.055 * pow(linearColor, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, linearColor));
    float alpha = pathOpacity * (trailStyle > 2.5 && trailStyle < 3.5 ? clamp(vFade, 0.0, 1.0) : 1.0);
    FRAGCOLOR = vec4(srgb, clamp(alpha, 0.0, 1.0));
}
