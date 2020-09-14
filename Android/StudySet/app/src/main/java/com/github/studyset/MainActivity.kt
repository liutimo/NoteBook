package com.github.studyset

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentStatePagerAdapter
import com.github.studyset.customview.CustomViewFragment
import com.github.studyset.viewpager.ViewPagerFragment
import com.google.android.material.tabs.TabLayout
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : AppCompatActivity() {


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        setSupportActionBar(toolbar)

        viewPager.adapter = ViewPagerFragmentAdapter(supportFragmentManager)

//        for (title in titleList) {
//            tabs.addTab(tabs.newTab().setText(title))
//        }

        // viewpage 和 tabLayout 联动
//        viewPager.addOnPageChangeListener(TabLayout.TabLayoutOnPageChangeListener(tabs))

        tabs.setupWithViewPager(viewPager)
    }



    companion object {
        val classList = listOf(
            ViewPagerFragment::class.java,
            CustomViewFragment::class.java
        )


        val titleList = listOf(
            "ViewPager1",
            "CustomView"
        )
    }


    inner class ViewPagerFragmentAdapter(fm: FragmentManager)
        : FragmentStatePagerAdapter( fm, BEHAVIOR_RESUME_ONLY_CURRENT_FRAGMENT ) {

        override fun getCount(): Int = classList.size

        override fun getItem(position: Int): Fragment = classList[position].newInstance()

        override fun getPageTitle(position: Int): CharSequence = titleList[position]
    }

}