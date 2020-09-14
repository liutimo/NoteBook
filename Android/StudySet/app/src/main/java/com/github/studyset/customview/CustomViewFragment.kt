package com.github.studyset.customview

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.github.studyset.R

class CustomViewFragment : Fragment(), View.OnClickListener {

    private lateinit var btnDrawLine: Button

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.custom_view_fragment_layout, container, false)
        btnDrawLine = view.findViewById(R.id.draw_line)
        btnDrawLine.setOnClickListener(this)
        return view
    }

    override fun onClick(p0: View) {
        when (p0.id) {
            R.id.draw_line -> {
                Toast.makeText(context, "DrawLine", Toast.LENGTH_SHORT).show()
            }
            else -> {

            }
        }
    }

}