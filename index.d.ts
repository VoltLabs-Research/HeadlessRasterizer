export interface RasterizeOptions {
    /** Vertical field of view in degrees. */
    fov?: number;
    /** Multiplier on the camera distance that frames the whole cloud. */
    distScale?: number;
    /** Z is up when true, Y otherwise. */
    zUp?: boolean;
}

/**
 * Renders a point cloud to a PNG. positions: xyz per point; colors: RGBA8 per point, or null
 * for grey points. Returns whether the PNG was written.
 */
export function rasterizePoints(
    positions: Float32Array,
    colors: Uint8Array | null,
    pngPath: string,
    width: number,
    height: number,
    azimuthDeg: number,
    elevationDeg: number,
    options: RasterizeOptions
): boolean;
