#ifndef __FREQUENCY_FORMATTER_HXX
#define __FREQUENCY_FORMATTER_HXX

/* ------------- A NAV/COMM Frequency formatter ---------------------- */

class FrequencyFormatter : public SGPropertyChangeListener {
public:
  FrequencyFormatter( SGPropertyNode_ptr freqNode, SGPropertyNode_ptr fmtFreqNode, double channelSpacing ) :
    _freqNode( freqNode ),
    _fmtFreqNode( fmtFreqNode ),
    _channelSpacing(channelSpacing)
  {
    _freqNode->addChangeListener( this );
    valueChanged(_freqNode);
  }
  ~FrequencyFormatter()
  {
    _freqNode->removeChangeListener( this );
  }

  void valueChanged (SGPropertyNode * prop)
  {
    // format as fixed decimal "nnn.nn"
    std::ostringstream buf;
    buf << std::fixed 
        << std::setw(5) 
        << std::setfill('0') 
        << std::setprecision(2)
        << getFrequency();
    _fmtFreqNode->setStringValue( buf.str() );
  }

  double getFrequency() const 
  {
    double d = SGMiscd::roundToInt(_freqNode->getDoubleValue() / _channelSpacing) * _channelSpacing;
    // strip last digit, do not round
    return ((int)(d*100))/100.0;
  }

private:
  SGPropertyNode_ptr _freqNode;
  SGPropertyNode_ptr _fmtFreqNode;
  double _channelSpacing;
};


#endif //__FREQUENCY_FORMATTER_HXX