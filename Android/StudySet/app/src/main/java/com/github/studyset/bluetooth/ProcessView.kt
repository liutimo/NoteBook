package com.github.studyset.bluetooth

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import androidx.constraintlayout.widget.ConstraintLayout
import com.github.studyset.R
import kotlinx.android.synthetic.main.process_view_layout.view.*

class ProcessView : ConstraintLayout {



    constructor(ctx: Context, attrs: AttributeSet) : super(ctx, attrs) {
        LayoutInflater.from(ctx).inflate(R.layout.process_view_layout, this)
    }

    fun setSpeed(speed: Int) {
        tvSpeed.text = String.format("%dKb/s", speed)
    }

}