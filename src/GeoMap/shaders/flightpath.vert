// Flight-path ribbon vertex shader; UV1 carries cumulative path distance and fade.
VARYING float vHighlight;
VARYING float vDistancePx;
VARYING float vFade;
VARYING float vSide;

void MAIN()
{
    vHighlight = UV0.y;
    vFade = UV1.y;
    vSide = UV0.x;
    vec3 worldCenter = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    vec3 viewVec = worldCenter - CAMERA_POSITION;
    float dist = length(viewVec);
    float unitsPerPixel = max(screenFactor * dist, 1e-6);
    vDistancePx = UV1.x / unitsPerPixel;
    vec3 worldTangent = (MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz;
    vec3 perp = cross(worldTangent, viewVec / max(dist, 1e-6));
    float perpLen = length(perp);
    vec3 sideDir = (perpLen > 1e-4) ? (perp / perpLen)
                                    : vec3(VIEW_MATRIX[0][0], VIEW_MATRIX[1][0], VIEW_MATRIX[2][0]);
    vec3 worldPos = worldCenter + (sideDir * (UV0.x * 0.5 * lineWidth * unitsPerPixel));
    vec4 viewPos = VIEW_MATRIX * vec4(worldPos, 1.0);
    viewPos.xyz *= 1.0 - depthPull;
    POSITION = PROJECTION_MATRIX * viewPos;
}
