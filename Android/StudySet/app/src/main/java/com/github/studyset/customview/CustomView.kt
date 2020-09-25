package com.github.studyset.customview

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View

class CustomView : View {

    constructor(ctx: Context?) : super(ctx)

    constructor(ctx: Context, attrs: AttributeSet) : super(ctx, attrs)

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        //开启抗锯齿
        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.parseColor("#66CCFF")
            strokeWidth = 1f
            style = Paint.Style.STROKE
        }


        //将整个绘制区域绘制颜色，就是填充整个view 类似的还有 drawColor drawRGB
//        canvas.drawARGB(242, 242, 242, 242)


        canvas.apply {
            drawLine(50f, 50f, 300f, 300f, paint)
            drawCircle(450f, 150f, 150f, paint)
            drawRect(300f, 0f, 600f, 300f, paint)
            paint.strokeWidth = 30f
            paint.strokeCap = Paint.Cap.BUTT
            drawPoint(450f, 150f, paint)
            paint.strokeCap = Paint.Cap.ROUND
            drawPoint(300f, 0f, paint)
            paint.strokeCap = Paint.Cap.SQUARE
            drawPoint(600f, 300f, paint)
            paint.strokeWidth = 1f

            save()
            drawRoundRect(600f, 0f, 1200f, 300f, 30f, 30f, paint)
            drawOval(600f, 0f, 1200f, 300f, paint)
            restore()

            drawArc(1200f, 0f, 1500f, 300f, -90f, 90f, false, paint)
            save()
            paint.style = Paint.Style.FILL
            drawArc(1200f, 0f, 1500f, 300f, 90f, 90f, true, paint)
            restore()

            //drawPath   y == 300f
            val path = Path()
            paint.style = Paint.Style.STROKE
            path.addArc(0f, 300f, 200f, 500f, -225f, 225f)
            path.arcTo(200f, 300f, 400f, 500f, -180f, 225f, true)
            path.lineTo(200f, 642f)
            //当style 是 FILL 模式时，会自动填充子模型
            //
            path.close() //形成闭合图形

            drawPath(path, paint)


            val shader = LinearGradient(400f, 300f, 700f, 600f,
                Color.parseColor("#E91E63"),
                Color.parseColor("#2196F3"),
                Shader.TileMode.CLAMP)

            paint.style = Paint.Style.FILL_AND_STROKE
            paint.setShader(shader)
            drawCircle(500f, 400f, 100f,  paint)

        }
    }

}