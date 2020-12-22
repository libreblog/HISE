/*
BEGIN_TEST_DATA
  f: main
  ret: int
  args: int
  input: 12
  output: 182
  error: ""
  filename: "wrap/wrap_data2"
END_TEST_DATA
*/


/** A dummy object that expects a table. */
struct X
{
	DECLARE_NODE(X);

	static const int NumTables = 1;

	template <int P> void setParameter(double d)
	{
		
	}

	void setExternalData(const ExternalData& d, int index)
	{
		d.referBlockTo(f, 0);
	}
	
	block f;
};



struct LookupTable
{
	span<float, 12> data = { 182.0f };
};

LookupTable lut;



/** A data handler that forwards the table to the X object. */
struct DataHandler
{
	static const int NumTables = 1;
	static const int NumAudioFiles = 0;
	static const int NumSliderPacks = 0;

	DataHandler(X& obj)
	{
		
	}
	
	void setExternalData(X& obj, const ExternalData& d, int index)
	{
		if(index == 0)
		    obj.setExternalData(d, 0);
	}
	
	
};

wrap::data<X, DataHandler> mainObject;



int main(int input)
{
	ExternalData o(lut);
	mainObject.setExternalData(o, 0);
	
	return (int)mainObject.getWrappedObject().f[3];
}

