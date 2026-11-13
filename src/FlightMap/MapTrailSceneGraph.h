#pragma once

#include <QtCore/QPointF>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

class QSGNode;

/// GPU-backed 2D trail mesh used as a geographically anchored MapQuickItem
/// sourceItem. No Canvas/QImage allocation or per-point QML objects.
class MapTrailSceneGraph : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor strokeColor READ strokeColor WRITE setStrokeColor NOTIFY strokeColorChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    Q_PROPERTY(qreal lineOpacity READ lineOpacity WRITE setLineOpacity NOTIFY lineOpacityChanged)
    Q_PROPERTY(QString lineStyle READ lineStyle WRITE setLineStyle NOTIFY lineStyleChanged)

public:
    struct Primitive {
        QPointF start;
        QPointF end;
        qreal alphaStart = 1.0;
        qreal alphaEnd = 1.0;
        bool dot = false;
    };

    static constexpr int kMaxPrimitives = 4096;

    explicit MapTrailSceneGraph(QQuickItem* parent = nullptr);

    QColor strokeColor() const { return _strokeColor; }
    void setStrokeColor(const QColor& color);

    qreal lineWidth() const { return _lineWidth; }
    void setLineWidth(qreal width);

    qreal lineOpacity() const { return _lineOpacity; }
    void setLineOpacity(qreal opacity);

    QString lineStyle() const { return _lineStyle; }
    void setLineStyle(const QString& style);

    /// Positions are local sourceItem pixels; NaN coordinates break the path.
    Q_INVOKABLE void setProjectedPath(const QVariantList& points);
    Q_INVOKABLE void setFadeFactors(const QVariantList& factors);

    /// CPU geometry seam for tests. Dash/dot spacing is continuous per path.
    static QVector<Primitive> buildPrimitives(const QVector<QPointF>& points,
                                              const QVector<qreal>& fades,
                                              qreal width,
                                              const QString& style);

signals:
    void strokeColorChanged();
    void lineWidthChanged();
    void lineOpacityChanged();
    void lineStyleChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    QVector<QPointF> _points;
    QVector<qreal> _fades;
    QColor _strokeColor{255, 51, 51};
    qreal _lineWidth = 3.0;
    qreal _lineOpacity = 1.0;
    QString _lineStyle{QStringLiteral("solid")};
};
