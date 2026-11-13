#include "MapTrailSceneGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QtCore/QLineF>
#include <QtQuick/QSGGeometry>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGVertexColorMaterial>

namespace MapTrailSceneGraphDetail {

constexpr qreal kEpsilon = 1e-4;
constexpr int kCircleSlices = 8;

bool valid(const QPointF& point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

qreal fadeAt(const QVector<qreal>& factors, qsizetype index)
{
    if (index >= factors.size() || !std::isfinite(factors[index])) {
        return 1.0;
    }
    return std::clamp(factors[index], 0.0, 1.0);
}

QPointF along(const QPointF& from, const QPointF& to, qreal portion)
{
    return from + (to - from) * portion;
}

void appendQuad(QVector<QSGGeometry::ColoredPoint2D>& output,
                const MapTrailSceneGraph::Primitive& primitive,
                qreal width,
                const QColor& color,
                qreal opacity)
{
    const QPointF delta = primitive.end - primitive.start;
    const qreal length = std::hypot(delta.x(), delta.y());
    if (length <= kEpsilon) {
        return;
    }
    const QPointF normal(-delta.y() * width / (2 * length), delta.x() * width / (2 * length));
    const QPointF a = primitive.start - normal;
    const QPointF b = primitive.start + normal;
    const QPointF c = primitive.end - normal;
    const QPointF d = primitive.end + normal;

    const auto add = [&](const QPointF& point, qreal fade) {
        const qreal alpha = std::clamp(opacity * fade, 0.0, 1.0);
        const auto channel = [alpha](qreal linear) -> uchar {
            return static_cast<uchar>(std::lround(std::clamp(linear * alpha, 0.0, 1.0) * 255));
        };
        QSGGeometry::ColoredPoint2D vertex;
        vertex.set(static_cast<float>(point.x()), static_cast<float>(point.y()),
                   channel(color.redF()), channel(color.greenF()), channel(color.blueF()),
                   channel(alpha));
        output.append(vertex);
    };
    add(a, primitive.alphaStart);
    add(b, primitive.alphaStart);
    add(c, primitive.alphaEnd);
    add(b, primitive.alphaStart);
    add(d, primitive.alphaEnd);
    add(c, primitive.alphaEnd);
}

void appendDot(QVector<QSGGeometry::ColoredPoint2D>& output,
               const MapTrailSceneGraph::Primitive& primitive,
               qreal width,
               const QColor& color,
               qreal opacity)
{
    const qreal alpha = std::clamp(opacity * primitive.alphaStart, 0.0, 1.0);
    const auto channel = [alpha](qreal linear) -> uchar {
        return static_cast<uchar>(std::lround(std::clamp(linear * alpha, 0.0, 1.0) * 255));
    };
    const uchar red = channel(color.redF());
    const uchar green = channel(color.greenF());
    const uchar blue = channel(color.blueF());
    const uchar aa = channel(alpha);
    const qreal radius = width / 2;
    const auto add = [&](const QPointF& point) {
        QSGGeometry::ColoredPoint2D vertex;
        vertex.set(static_cast<float>(point.x()), static_cast<float>(point.y()), red, green, blue, aa);
        output.append(vertex);
    };

    for (int i = 0; i < kCircleSlices; ++i) {
        const qreal a = 2 * std::numbers::pi * i / kCircleSlices;
        const qreal b = 2 * std::numbers::pi * (i + 1) / kCircleSlices;
        add(primitive.start);
        add(primitive.start + QPointF(std::cos(a) * radius, std::sin(a) * radius));
        add(primitive.start + QPointF(std::cos(b) * radius, std::sin(b) * radius));
    }
}

} // namespace MapTrailSceneGraphDetail

MapTrailSceneGraph::MapTrailSceneGraph(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
}

void MapTrailSceneGraph::setStrokeColor(const QColor& color)
{
    if (_strokeColor != color) {
        _strokeColor = color;
        update();
        emit strokeColorChanged();
    }
}

void MapTrailSceneGraph::setLineWidth(qreal width)
{
    if (!std::isfinite(width)) {
        return;
    }
    width = std::clamp(width, 1.0, 12.0);
    if (_lineWidth != width) {
        _lineWidth = width;
        update();
        emit lineWidthChanged();
    }
}

void MapTrailSceneGraph::setLineOpacity(qreal opacity)
{
    if (!std::isfinite(opacity)) {
        return;
    }
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (_lineOpacity != opacity) {
        _lineOpacity = opacity;
        update();
        emit lineOpacityChanged();
    }
}

void MapTrailSceneGraph::setLineStyle(const QString& style)
{
    if (_lineStyle != style) {
        _lineStyle = style;
        update();
        emit lineStyleChanged();
    }
}

void MapTrailSceneGraph::setProjectedPath(const QVariantList& points)
{
    _points.clear();
    _points.reserve(std::min(points.size(), kMaxPrimitives + 1));
    // The monotonic history has at most 2048 points, and this additional guard
    // prevents an accidental unbounded QML input from reaching GPU geometry.
    for (qsizetype i = std::max(qsizetype(0), points.size() - qsizetype(kMaxPrimitives + 1));
         i < points.size(); ++i) {
        _points.append(points[i].toPointF());
    }
    update();
}

void MapTrailSceneGraph::setFadeFactors(const QVariantList& factors)
{
    _fades.clear();
    _fades.reserve(std::min(factors.size(), kMaxPrimitives + 1));
    for (qsizetype i = std::max(qsizetype(0), factors.size() - qsizetype(kMaxPrimitives + 1));
         i < factors.size(); ++i) {
        _fades.append(factors[i].toDouble());
    }
    update();
}

QVector<MapTrailSceneGraph::Primitive> MapTrailSceneGraph::buildPrimitives(
    const QVector<QPointF>& points,
    const QVector<qreal>& fades,
    qreal width,
    const QString& style)
{
    QVector<Primitive> result;
    if (points.size() < 2 || style == QLatin1String("none") || width <= 0) {
        return result;
    }
    result.reserve(std::min(int(points.size()), kMaxPrimitives));
    const bool dashed = style == QLatin1String("dashed");
    const bool dotted = style == QLatin1String("dotted");
    const bool fading = style == QLatin1String("fading");
    const qreal dashLength = 9.0;
    const qreal dashPeriod = dashLength + 6.0;
    const qreal dotPeriod = std::max(width * 2.5, 7.0);
    qreal accumulated = 0.0;

    for (qsizetype i = 1; i < points.size() && result.size() < kMaxPrimitives; ++i) {
        const QPointF& a = points[i - 1];
        const QPointF& b = points[i];
        if (!MapTrailSceneGraphDetail::valid(a) || !MapTrailSceneGraphDetail::valid(b)) {
            accumulated = 0;
            continue;
        }
        const qreal segmentLength = std::hypot(b.x() - a.x(), b.y() - a.y());
        if (segmentLength <= MapTrailSceneGraphDetail::kEpsilon) {
            continue;
        }
        const qreal startFade = fading ? MapTrailSceneGraphDetail::fadeAt(fades, i - 1) : 1.0;
        const qreal endFade = fading ? MapTrailSceneGraphDetail::fadeAt(fades, i) : 1.0;
        const auto add = [&](qreal from, qreal to, bool isDot) {
            const qreal startPart = from / segmentLength;
            const qreal endPart = to / segmentLength;
            result.append({
                .start = MapTrailSceneGraphDetail::along(a, b, startPart),
                .end = MapTrailSceneGraphDetail::along(a, b, endPart),
                .alphaStart = startFade + (endFade - startFade) * startPart,
                .alphaEnd = startFade + (endFade - startFade) * endPart,
                .dot = isDot,
            });
        };

        if (dashed) {
            qreal offset = 0;
            while (offset < segmentLength && result.size() < kMaxPrimitives) {
                const qreal phase = std::fmod(accumulated + offset, dashPeriod);
                const bool visible = phase < dashLength;
                const qreal step = std::max(0.001, visible ? dashLength - phase : dashPeriod - phase);
                const qreal until = std::min(segmentLength, offset + step);
                if (visible) {
                    add(offset, until, false);
                }
                offset = until;
            }
        } else if (dotted) {
            qreal first = std::fmod(accumulated, dotPeriod);
            qreal offset = first < MapTrailSceneGraphDetail::kEpsilon ? 0.0 : dotPeriod - first;
            for (; offset < segmentLength && result.size() < kMaxPrimitives; offset += dotPeriod) {
                add(offset, offset, true);
            }
        } else {
            add(0, segmentLength, false);
        }
        accumulated += segmentLength;
    }
    return result;
}

QSGNode* MapTrailSceneGraph::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }
    const auto primitives = buildPrimitives(_points, _fades, _lineWidth, _lineStyle);
    QVector<QSGGeometry::ColoredPoint2D> vertices;
    const bool dotted = _lineStyle == QLatin1String("dotted");
    vertices.reserve(primitives.size() * (dotted ? 24 : 6));
    if (_lineOpacity > 0.0 && _strokeColor.isValid()) {
        for (const Primitive& primitive : primitives) {
            if (primitive.dot) {
                MapTrailSceneGraphDetail::appendDot(vertices, primitive, _lineWidth, _strokeColor, _lineOpacity);
            } else {
                MapTrailSceneGraphDetail::appendQuad(vertices, primitive, _lineWidth, _strokeColor, _lineOpacity);
            }
        }
    }
    auto* geometry = node->geometry();
    geometry->allocate(vertices.size());
    std::copy(vertices.cbegin(), vertices.cend(), geometry->vertexDataAsColoredPoint2D());
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
