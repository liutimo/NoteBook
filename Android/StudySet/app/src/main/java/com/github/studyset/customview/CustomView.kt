package com.github.studyset.customview

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
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
        }
    }

}